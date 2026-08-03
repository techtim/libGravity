/**
 * Host unit tests for the GravityPlus firmware's single gate Channel.
 *
 * firmware/Euclidean is on the native -I path and shares the "channel.h"
 * filename, so we point explicitly at the GravityPlus one; its same-directory
 * quote-includes (clock_mod.h) resolve the rest.
 */
#include <ArduinoFake.h>
#include <unity.h>

#include "arduino_compat.h" // Arduino min()/max() the mock omits
#include "../../firmware/GravityPlus/channel.h"

using namespace fakeit;

void setUp() {
  ArduinoFakeReset();
  When(Method(ArduinoFake(), pinMode)).AlwaysReturn();
  When(Method(ArduinoFake(), digitalWrite)).AlwaysReturn();
  When(Method(ArduinoFake(), map))
      .AlwaysDo([](long x, long in_min, long in_max, long out_min,
                   long out_max) -> long {
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
      });
}
void tearDown() {}

// Six named params, with the documented defaults.
void test_defaults(void) {
  Channel ch;
  TEST_ASSERT_EQUAL_INT(1, ch.paramValue(CP_STEPS, false));
  TEST_ASSERT_EQUAL_INT(1, ch.paramValue(CP_HITS, false));
  TEST_ASSERT_EQUAL_INT(100, ch.paramValue(CP_PROB, false));
  TEST_ASSERT_EQUAL_INT(50, ch.paramValue(CP_DUTY, false));
  TEST_ASSERT_EQUAL_INT(0, ch.paramValue(CP_OFFSET, false));
  TEST_ASSERT_EQUAL_INT(50, ch.paramValue(CP_SWING, false));
}

// HITS clamps to STEPS both when editing HITS up and when shrinking STEPS.
void test_hits_clamped_to_steps(void) {
  Channel ch;
  ch.editParam(CP_STEPS, 7); // steps -> 8
  ch.editParam(CP_HITS, 20); // hits requested 21 -> clamp to 8
  TEST_ASSERT_EQUAL_INT(8, ch.paramValue(CP_HITS, false));
  ch.editParam(CP_STEPS, -5); // steps -> 3, hits should follow down
  TEST_ASSERT_EQUAL_INT(3, ch.paramValue(CP_HITS, false));
}

// Euclidean E(4,2) at the smallest clock division (mod_pulses = 4) rises on
// ticks 0 and 8 across one 16-tick window (steps 0 and 2 are the hits).
void test_euclidean_gate_pattern(void) {
  Channel ch;
  ch.setClockMod(MOD_CHOICE_SIZE - 1);
  ch.editParam(CP_STEPS, 3); // steps -> 4
  ch.editParam(CP_HITS, 1);  // hits -> 2

  DigitalOutput out;
  out.Init(7);
  int rise_ticks[8];
  int rc = 0;
  bool prev = false;
  for (uint32_t t = 0; t < 16; t++) {
    ch.processClockTick(t, out);
    bool on = out.On();
    if (on && !prev && rc < 8)
      rise_ticks[rc++] = t;
    prev = on;
  }
  TEST_ASSERT_EQUAL_INT(2, rc);
  TEST_ASSERT_EQUAL_INT(0, rise_ticks[0]);
  TEST_ASSERT_EQUAL_INT(8, rise_ticks[1]);
}

// Probability replacement: E(1,1) fires every step; DUTY sets the gate length
// and OFFSET shifts the rising edge. mod_pulses = 96 (x1). prob = 100 skips the
// RNG, so these edges are fully deterministic.
void test_probability_gate_edges(void) {
  Channel ch; // steps=hits=1, prob=100, duty=50, offset=0
  DigitalOutput out;
  out.Init(7);
  int rise = -1, fall = -1;
  bool prev = false;
  for (uint32_t t = 0; t < 96; t++) {
    ch.processClockTick(t, out);
    bool on = out.On();
    if (on && !prev)
      rise = t;
    if (!on && prev)
      fall = t;
    prev = on;
  }
  TEST_ASSERT_EQUAL_INT(0, rise);  // opens at phase 0
  TEST_ASSERT_EQUAL_INT(48, fall); // 50% duty of a 96-pulse window

  // Offset shifts the rising edge to phase 24 (offset 25% of 96).
  Channel off;
  off.editParam(CP_OFFSET, 25);
  DigitalOutput o2;
  o2.Init(7);
  int rise2 = -1;
  prev = false;
  for (uint32_t t = 0; t < 96; t++) {
    off.processClockTick(t, o2);
    if (o2.On() && !prev)
      rise2 = t;
    prev = o2.On();
  }
  TEST_ASSERT_EQUAL_INT(24, rise2);
}

// tick == 0 (a clock start/reset) returns the pattern to step 0.
void test_restart_to_step0(void) {
  Channel ch;
  ch.setClockMod(MOD_CHOICE_SIZE - 1);
  ch.editParam(CP_STEPS, 3); // E(4,2)
  ch.editParam(CP_HITS, 1);
  DigitalOutput out;
  out.Init(7);
  ch.processClockTick(0, out);
  ch.processClockTick(1, out); // advance into the pattern
  ch.processClockTick(0, out); // restart
  TEST_ASSERT_TRUE(out.On());  // step 0 is a hit -> fires immediately
}

// A muted channel always drives its output low.
void test_mute_forces_low(void) {
  Channel ch;
  ch.setMute(true);
  DigitalOutput out;
  out.Init(7);
  for (uint32_t t = 0; t < 96; t++) {
    ch.processClockTick(t, out);
    TEST_ASSERT_FALSE(out.On());
  }
}

// CV routed to STEPS modulates only STEPS; the display getter shows the
// modulated value for the routed param and base for the others.
void test_cv_param_targeting(void) {
  Channel ch;
  ch.editParam(CP_STEPS, 3); // base steps = 4
  TEST_ASSERT_EQUAL_INT(4, ch.paramValue(CP_STEPS, true)); // not routed -> base

  ch.setCvDest(0, CV_STEPS); // slot CV1-A -> STEPS
  ch.setCvAmount(0, 100);
  ch.applyCvMod(127, 0); // steps 4 + 127*100/128 = 103 -> clamp MAX_PATTERN_STEPS
  TEST_ASSERT_EQUAL_INT(16, ch.paramValue(CP_STEPS, true)); // routed -> live
  TEST_ASSERT_EQUAL_INT(4, ch.paramValue(CP_STEPS, false)); // base unchanged
  TEST_ASSERT_EQUAL_INT(ch.paramValue(CP_PROB, false),
                        ch.paramValue(CP_PROB, true)); // others untouched
}

// A CV routed to CLOCK MOD must move the *displayed* index too, not just the
// internal pulse count. Regression: targetsParam() was called with CV_CLOCK_MOD
// instead of CP_CLOCK_MOD, so getClockModIndex(true) kept returning the base.
void test_cv_targets_clock_mod(void) {
  Channel ch;
  ch.setClockMod(16); // x1
  // Not routed: the modulated getter shows the base index.
  TEST_ASSERT_EQUAL_INT(16, ch.getClockModIndex(true));

  ch.setCvDest(0, CV_CLOCK_MOD); // slot CV1-A
  ch.setCvAmount(0, -100);
  // 127 * -100 / 128 = -99; -99 * 12 / 100 = -11  =>  16 - 11 = 5
  ch.applyCvMod(127, 0);
  TEST_ASSERT_EQUAL_INT(5, ch.getClockModIndex(true));  // modulated
  TEST_ASSERT_EQUAL_INT(16, ch.getClockModIndex(false)); // base untouched
  // The clock-mod value shown follows the modulated index.
  TEST_ASSERT_EQUAL_INT(clockModValue(5), ch.getClockMod(true));
}

// A CV routed to HITS must actually change the drawn pattern, not just live_.
void test_cv_hits_redraws_pattern(void) {
  Channel ch;
  ch.editParam(CP_STEPS, 7); // 8 steps
  ch.editParam(CP_HITS, 1);  // 2 hits
  ch.setCvDest(0, CV_HITS);
  ch.setCvAmount(0, 100);

  ch.applyCvMod(0, 0); // no CV: pattern is E(2,8)
  uint8_t hits_at_zero = 0;
  for (uint8_t i = 0; i < ch.patternSteps(); i++)
    hits_at_zero += ch.patternHit(i) ? 1 : 0;
  TEST_ASSERT_EQUAL_INT(2, hits_at_zero);

  ch.applyCvMod(127, 0); // full CV: +15 hits, clamped to steps
  uint8_t hits_at_full = 0;
  for (uint8_t i = 0; i < ch.patternSteps(); i++)
    hits_at_full += ch.patternHit(i) ? 1 : 0;
  TEST_ASSERT_EQUAL_INT(8, hits_at_full);
}

// Two further destinations, one per CV input: CV1-B -> DUTY, CV2-A -> PROB.
// Each moves only its own param; unrouted params still read as base.
void test_cv_targets_duty_and_prob(void) {
  Channel ch; // duty 50, prob 100, offset 0
  ch.setCvDest(1, CV_DUTY); // slot CV1-B reads cv1
  ch.setCvAmount(1, 40);
  ch.setCvDest(2, CV_PROB); // slot CV2-A reads cv2
  ch.setCvAmount(2, -50);

  ch.applyCvMod(127, 127);
  // duty: 127 * 40 / 128 = 39  => 50 + 39 = 89
  TEST_ASSERT_EQUAL_INT(89, ch.paramValue(CP_DUTY, true));
  // prob: 127 * -50 / 128 = -49 => 100 - 49 = 51
  TEST_ASSERT_EQUAL_INT(51, ch.paramValue(CP_PROB, true));
  // Bases unchanged.
  TEST_ASSERT_EQUAL_INT(50, ch.paramValue(CP_DUTY, false));
  TEST_ASSERT_EQUAL_INT(100, ch.paramValue(CP_PROB, false));
  // An unrouted param reads the same either way.
  TEST_ASSERT_EQUAL_INT(ch.paramValue(CP_OFFSET, false),
                        ch.paramValue(CP_OFFSET, true));
  // CLOCK MOD is not routed here, so it must not be flagged as modulated.
  TEST_ASSERT_EQUAL_INT(ch.getClockModIndex(false), ch.getClockModIndex(true));
}

// Run `ticks` clock ticks and report the FIRST rising edge and the first
// falling edge after it (-1 if the gate never opened / never closed). prob=100
// so the RNG is skipped and the edges are deterministic.
static void gateEdges(Channel &ch, uint32_t ticks, int &rise, int &fall) {
  DigitalOutput out;
  out.Init(7);
  rise = -1;
  fall = -1;
  bool prev = false;
  for (uint32_t t = 0; t < ticks; t++) {
    ch.processClockTick(t, out);
    bool on = out.On();
    if (on && !prev && rise < 0)
      rise = (int)t;
    if (!on && prev && rise >= 0 && fall < 0)
      fall = (int)t;
    prev = on;
  }
}

// OFFSET 98% pushes the gate almost a whole step late, so it opens near the end
// of one step and closes inside the next. mod = 96: offset_pulses = 96*2/100 = 1
// -> opens at phase 95; duty 50% closes 48 pulses later, at phase 47 of the next
// window (tick 143). The gate length must still be the full 50% duty.
void test_gate_offset_98(void) {
  Channel ch;
  ch.editParam(CP_OFFSET, 98);
  TEST_ASSERT_EQUAL_INT(98, ch.paramValue(CP_OFFSET, false));
  int rise, fall;
  gateEdges(ch, 192, rise, fall); // two windows: the gate wraps the boundary
  TEST_ASSERT_EQUAL_INT(95, rise);
  TEST_ASSERT_EQUAL_INT(143, fall);
  TEST_ASSERT_EQUAL_INT(48, fall - rise); // 50% of a 96-pulse step
}

// Edge phases must wrap to 0 when the shift lands on a whole step, otherwise the
// computed phase equals mod - which `phase` (0..mod-1) never reaches - and the
// gate sticks. Regression: (mod - shift % mod) lost its outer % mod.
void test_gate_edges_wrap_to_zero(void) {
  // duty 50 + offset 50 -> close shift = 48 + 48 = 96 = one whole step.
  Channel a;
  a.editParam(CP_OFFSET, 50);
  int rise, fall;
  gateEdges(a, 192, rise, fall);
  TEST_ASSERT_EQUAL_INT(48, rise);
  TEST_ASSERT_EQUAL_INT(96, fall); // must close, not stay high forever

  // offset 99 -> offset_pulses = 96*1/100 = 0 -> opens on phase 0.
  Channel b;
  b.editParam(CP_OFFSET, 99);
  gateEdges(b, 192, rise, fall);
  TEST_ASSERT_EQUAL_INT(0, rise); // must open, not stay low forever
  TEST_ASSERT_EQUAL_INT(48, fall);
}

// Full save/load round-trip through the raw byte payload (choke included).
void test_save_load_roundtrip(void) {
  Channel ch;
  ch.setClockMod(5);
  ch.editParam(CP_STEPS, 4); // steps 5
  ch.editParam(CP_HITS, 2);  // hits 3
  ch.editParam(CP_DUTY, -20); // duty 30
  ch.setCvDest(2, CV_OFFSET); // slot CV2-A -> OFFSET
  ch.setCvAmount(2, -40);
  ch.setMute(true);
  ch.setChoke(3);

  byte payload[Channel::SAVE_BYTES] = {0};
  ch.save(payload);

  Channel loaded;
  loaded.load(payload);
  TEST_ASSERT_EQUAL_INT(5, loaded.paramValue(CP_STEPS, false));
  TEST_ASSERT_EQUAL_INT(3, loaded.paramValue(CP_HITS, false));
  TEST_ASSERT_EQUAL_INT(30, loaded.paramValue(CP_DUTY, false));
  TEST_ASSERT_EQUAL_INT(5, loaded.getClockModIndex(false));
  TEST_ASSERT_EQUAL(CV_OFFSET, loaded.getCvDest(2));
  TEST_ASSERT_EQUAL_INT(-40, loaded.getCvAmount(2));
  TEST_ASSERT_TRUE(loaded.isMuted());
  TEST_ASSERT_EQUAL_UINT8(3, loaded.getChoke());
}

// Choke defaults off and stores a 1-based source channel.
void test_choke_field(void) {
  Channel ch;
  TEST_ASSERT_EQUAL_UINT8(0, ch.getChoke());
  ch.setChoke(4);
  TEST_ASSERT_EQUAL_UINT8(4, ch.getChoke());
}

// ROTATE cyclically shifts the euclidean pattern. E(5,2) = X__X_; rotate 1 =
// _X__X; rotate 2 = X_X__.
void test_pattern_rotate(void) {
  Channel ch;
  ch.editParam(CP_STEPS, 4); // 1 -> 5
  ch.editParam(CP_HITS, 1);  // 1 -> 2
  const bool base[5] = {true, false, false, true, false}; // X__X_
  for (uint8_t i = 0; i < 5; i++)
    TEST_ASSERT_EQUAL(base[i], ch.patternHit(i));

  ch.editParam(CP_ROTATE, 1); // _X__X
  const bool r1[5] = {false, true, false, false, true};
  for (uint8_t i = 0; i < 5; i++)
    TEST_ASSERT_EQUAL(r1[i], ch.patternHit(i));

  ch.editParam(CP_ROTATE, 1); // rotate = 2 -> X_X__
  const bool r2[5] = {true, false, true, false, false};
  for (uint8_t i = 0; i < 5; i++)
    TEST_ASSERT_EQUAL(r2[i], ch.patternHit(i));
}

// Shrinking STEPS clamps HITS; growing STEPS back must NOT resurrect the old
// hit count (live_ must track base_). E(6,6) -> steps 1 -> steps 6 = E(6,1).
void test_steps_shrink_grow_keeps_hits(void) {
  Channel ch;
  ch.editParam(CP_STEPS, 5); // 1 -> 6
  ch.editParam(CP_HITS, 5);  // 1 -> 6  (E(6,6), all hits)
  ch.editParam(CP_STEPS, -5); // 6 -> 1, hits clamps to 1
  ch.editParam(CP_STEPS, 5);  // 1 -> 6, hits stays 1
  TEST_ASSERT_EQUAL_INT(1, ch.paramValue(CP_HITS, false));
  uint8_t hits = 0;
  for (uint8_t i = 0; i < 6; i++)
    if (ch.patternHit(i))
      hits++;
  TEST_ASSERT_EQUAL_INT(1, hits); // E(6,1) = single hit
}

// Gate edges at a SLOW clock division (index 13 = /4, mod_pulses = 384). This is
// where the finalize() phase math overflowed a 16-bit int (mod * (100-x)).
// E(1,1): opens at phase 0, closes at 50% duty = 192.
void test_gate_edges_slow_div(void) {
  Channel ch;
  ch.setClockMod(13); // /4 -> 384 pulses per step
  int rise, fall;
  gateEdges(ch, 384, rise, fall);
  TEST_ASSERT_EQUAL_INT(0, rise);
  TEST_ASSERT_EQUAL_INT(192, fall);
}

// OFFSET shifts the rising edge; DUTY shifts the falling edge - at the slow
// division where 16-bit overflow would corrupt them.
void test_gate_offset_duty_slow_div(void) {
  Channel a;
  a.setClockMod(13);
  a.editParam(CP_OFFSET, 25); // 25% of 384 = 96
  int rise, fall;
  gateEdges(a, 384, rise, fall);
  TEST_ASSERT_EQUAL_INT(96, rise);

  Channel b;
  b.setClockMod(13);
  b.editParam(CP_DUTY, -25); // duty 50 -> 25; close at 25% of 384 = 96
  gateEdges(b, 384, rise, fall);
  TEST_ASSERT_EQUAL_INT(0, rise);
  TEST_ASSERT_EQUAL_INT(96, fall);
}

// SWING delays the gate on odd steps. swing 75 (shift 25%) at mod 96: even step
// opens at phase 0, odd step opens at phase 24.
void test_gate_swing(void) {
  Channel ch;
  ch.editParam(CP_SWING, 25); // 50 -> 75
  DigitalOutput out;
  out.Init(7);
  int r_even = -1, r_odd = -1;
  bool prev = false;
  for (uint32_t t = 0; t < 192; t++) { // two 96-pulse windows
    ch.processClockTick(t, out);
    bool on = out.On();
    if (on && !prev) {
      if (t < 96)
        r_even = (int)t;
      else
        r_odd = (int)(t - 96);
    }
    prev = on;
  }
  TEST_ASSERT_EQUAL_INT(0, r_even);
  TEST_ASSERT_EQUAL_INT(24, r_odd);
}

// The choke rule that HandleIntClockTick applies: a channel whose choke source's
// gate is high is forced low. Replicated here over two DigitalOutputs.
void test_choke_silences_when_source_on(void) {
  DigitalOutput a, b;
  a.Init(7);
  b.Init(8);
  a.High();       // source (channel 1) gate open
  b.High();       // target decided to fire
  Channel tgt;
  tgt.setChoke(1); // choke by channel 1
  // Apply the rule (mirror of HandleIntClockTick).
  bool src_on = a.On();
  if (tgt.getChoke() != 0 && src_on)
    b.Low();
  TEST_ASSERT_FALSE(b.On()); // choked off

  // Source low -> target survives.
  a.Low();
  b.High();
  src_on = a.On();
  if (tgt.getChoke() != 0 && src_on)
    b.Low();
  TEST_ASSERT_TRUE(b.On());
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_defaults);
  RUN_TEST(test_hits_clamped_to_steps);
  RUN_TEST(test_euclidean_gate_pattern);
  RUN_TEST(test_probability_gate_edges);
  RUN_TEST(test_restart_to_step0);
  RUN_TEST(test_mute_forces_low);
  RUN_TEST(test_cv_param_targeting);
  RUN_TEST(test_cv_targets_clock_mod);
  RUN_TEST(test_cv_targets_duty_and_prob);
  RUN_TEST(test_cv_hits_redraws_pattern);
  RUN_TEST(test_save_load_roundtrip);
  RUN_TEST(test_choke_field);
  RUN_TEST(test_pattern_rotate);
  RUN_TEST(test_gate_offset_98);
  RUN_TEST(test_gate_edges_wrap_to_zero);
  RUN_TEST(test_gate_edges_slow_div);
  RUN_TEST(test_gate_offset_duty_slow_div);
  RUN_TEST(test_gate_swing);
  RUN_TEST(test_steps_shrink_grow_keeps_hits);
  RUN_TEST(test_choke_silences_when_source_on);
  return UNITY_END();
}
