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
  TEST_ASSERT_EQUAL_UINT8(GATE_COUNT, ch.paramCount());
  TEST_ASSERT_EQUAL_UINT8(7, ch.paramCount());
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
  ch.applyCvMod(512, 0); // steps 4 + 512*100/512 = 104 -> clamp 32
  TEST_ASSERT_EQUAL_INT(32, ch.paramValue(CP_STEPS, true)); // routed -> live
  TEST_ASSERT_EQUAL_INT(4, ch.paramValue(CP_STEPS, false)); // base unchanged
  TEST_ASSERT_EQUAL_INT(ch.paramValue(CP_PROB, false),
                        ch.paramValue(CP_PROB, true)); // others untouched
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
  RUN_TEST(test_save_load_roundtrip);
  RUN_TEST(test_choke_field);
  RUN_TEST(test_pattern_rotate);
  RUN_TEST(test_choke_silences_when_source_on);
  return UNITY_END();
}
