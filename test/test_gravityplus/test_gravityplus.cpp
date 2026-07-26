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
  TEST_ASSERT_EQUAL_UINT8(GATE_PARAM_COUNT, ch.paramCount());
  TEST_ASSERT_EQUAL_UINT8(6, ch.paramCount());
  TEST_ASSERT_EQUAL_INT(1, ch.paramValue(GATE_STEPS, false));
  TEST_ASSERT_EQUAL_INT(1, ch.paramValue(GATE_HITS, false));
  TEST_ASSERT_EQUAL_INT(100, ch.paramValue(GATE_PROB, false));
  TEST_ASSERT_EQUAL_INT(50, ch.paramValue(GATE_DUTY, false));
  TEST_ASSERT_EQUAL_INT(0, ch.paramValue(GATE_OFFSET, false));
  TEST_ASSERT_EQUAL_INT(50, ch.paramValue(GATE_SWING, false));
}

// HITS clamps to STEPS both when editing HITS up and when shrinking STEPS.
void test_hits_clamped_to_steps(void) {
  Channel ch;
  ch.editParam(GATE_STEPS, 7); // steps -> 8
  ch.editParam(GATE_HITS, 20); // hits requested 21 -> clamp to 8
  TEST_ASSERT_EQUAL_INT(8, ch.paramValue(GATE_HITS, false));
  ch.editParam(GATE_STEPS, -5); // steps -> 3, hits should follow down
  TEST_ASSERT_EQUAL_INT(3, ch.paramValue(GATE_HITS, false));
}

// Euclidean E(4,2) at the smallest clock division (mod_pulses = 4) rises on
// ticks 0 and 8 across one 16-tick window (steps 0 and 2 are the hits).
void test_euclidean_gate_pattern(void) {
  Channel ch;
  ch.setClockMod(MOD_CHOICE_SIZE - 1);
  ch.editParam(GATE_STEPS, 3); // steps -> 4
  ch.editParam(GATE_HITS, 1);  // hits -> 2

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
  off.editParam(GATE_OFFSET, 25);
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
  ch.editParam(GATE_STEPS, 3); // E(4,2)
  ch.editParam(GATE_HITS, 1);
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
  ch.editParam(GATE_STEPS, 3); // base steps = 4
  TEST_ASSERT_EQUAL_INT(4, ch.paramValue(GATE_STEPS, true)); // not routed -> base

  ch.setCv1Target(CV_STEPS);
  ch.applyCvMod(512, 0); // steps 4 + map(512,-512,512,0,32)=32 -> clamp 32
  TEST_ASSERT_EQUAL_INT(32, ch.paramValue(GATE_STEPS, true)); // routed -> live
  TEST_ASSERT_EQUAL_INT(4, ch.paramValue(GATE_STEPS, false)); // base unchanged
  TEST_ASSERT_EQUAL_INT(ch.paramValue(GATE_PROB, false),
                        ch.paramValue(GATE_PROB, true)); // others untouched
}

// Full save/load round-trip through the raw byte payload.
void test_save_load_roundtrip(void) {
  Channel ch;
  ch.setClockMod(5);
  ch.editParam(GATE_STEPS, 4); // steps 5
  ch.editParam(GATE_HITS, 2);  // hits 3
  ch.editParam(GATE_DUTY, -20); // duty 30
  ch.setCv1Target(CV_OFFSET);
  ch.setMute(true);

  byte payload[Channel::SAVE_BYTES] = {0};
  ch.save(payload);

  Channel loaded;
  loaded.load(payload);
  TEST_ASSERT_EQUAL_INT(5, loaded.paramValue(GATE_STEPS, false));
  TEST_ASSERT_EQUAL_INT(3, loaded.paramValue(GATE_HITS, false));
  TEST_ASSERT_EQUAL_INT(30, loaded.paramValue(GATE_DUTY, false));
  TEST_ASSERT_EQUAL_INT(5, loaded.getClockModIndex(false));
  TEST_ASSERT_EQUAL(CV_OFFSET, loaded.getCv1Target());
  TEST_ASSERT_TRUE(loaded.isMuted());
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
  return UNITY_END();
}
