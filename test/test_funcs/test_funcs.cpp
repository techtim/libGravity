/**
 * Host unit tests for the GravityPlus Channel's parameter model.
 *
 * GravityPlus has a single generator - no func union, no mode switching - so
 * this suite covers what used to be the per-func behaviour: the defaults, each
 * param's range, the euclidean pattern the params produce, and how a routed CV
 * modulates one param without touching the others. The gate edge phases and the
 * CV/save plumbing are covered in test_gravityplus.
 *
 * Uses a relative include for the entry header: firmware/Euclidean is already on
 * the native -I path and shares the "channel.h" filename, so we point explicitly
 * at the GravityPlus one. Its own same-directory quote-includes resolve the rest.
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

// A fresh channel is a single-step gate that always fires: STEPS = HITS = 1 and
// PROB = 100 is what makes this one generator subsume the old probability func.
void test_defaults(void) {
  Channel ch;
  TEST_ASSERT_EQUAL_INT(1, ch.paramValue(CP_STEPS, false));
  TEST_ASSERT_EQUAL_INT(1, ch.paramValue(CP_HITS, false));
  TEST_ASSERT_EQUAL_INT(0, ch.paramValue(CP_ROTATE, false));
  TEST_ASSERT_EQUAL_INT(100, ch.paramValue(CP_PROB, false));
  TEST_ASSERT_EQUAL_INT(50, ch.paramValue(CP_DUTY, false));
  TEST_ASSERT_EQUAL_INT(0, ch.paramValue(CP_OFFSET, false));
  TEST_ASSERT_EQUAL_INT(50, ch.paramValue(CP_SWING, false)); // 50 = no swing
  TEST_ASSERT_EQUAL_INT(1, ch.patternSteps());
  TEST_ASSERT_TRUE(ch.patternHit(0));
}

// Each param clamps to its own range. Driving well past both ends is what
// catches an off-by-one or a missing bound.
void test_param_ranges(void) {
  Channel ch;
  ch.editParam(CP_STEPS, 99);
  TEST_ASSERT_EQUAL_INT(MAX_PATTERN_STEPS, ch.paramValue(CP_STEPS, false));
  ch.editParam(CP_STEPS, -99);
  TEST_ASSERT_EQUAL_INT(1, ch.paramValue(CP_STEPS, false)); // never 0 steps

  ch.editParam(CP_STEPS, 7); // 8 steps, so HITS may reach 8
  ch.editParam(CP_HITS, 99);
  TEST_ASSERT_EQUAL_INT(8, ch.paramValue(CP_HITS, false)); // bounded by STEPS
  ch.editParam(CP_HITS, -99);
  TEST_ASSERT_EQUAL_INT(1, ch.paramValue(CP_HITS, false));

  ch.editParam(CP_ROTATE, 99);
  TEST_ASSERT_EQUAL_INT(7, ch.paramValue(CP_ROTATE, false)); // steps - 1

  ch.editParam(CP_PROB, 99);
  TEST_ASSERT_EQUAL_INT(100, ch.paramValue(CP_PROB, false));
  ch.editParam(CP_PROB, -999);
  TEST_ASSERT_EQUAL_INT(0, ch.paramValue(CP_PROB, false));

  ch.editParam(CP_DUTY, 99); // duty never reaches 100 - the gate must close
  TEST_ASSERT_EQUAL_INT(99, ch.paramValue(CP_DUTY, false));
  ch.editParam(CP_DUTY, -999);
  TEST_ASSERT_EQUAL_INT(1, ch.paramValue(CP_DUTY, false)); // nor 0

  ch.editParam(CP_OFFSET, 999);
  TEST_ASSERT_EQUAL_INT(99, ch.paramValue(CP_OFFSET, false));

  ch.editParam(CP_SWING, 999);
  TEST_ASSERT_EQUAL_INT(95, ch.paramValue(CP_SWING, false));
  ch.editParam(CP_SWING, -999);
  TEST_ASSERT_EQUAL_INT(50, ch.paramValue(CP_SWING, false));
}

// E(4,2) puts hits on steps 0 and 2 - the classic evenly-spread pattern.
void test_euclidean_pattern_spread(void) {
  Channel ch;
  ch.editParam(CP_STEPS, 3); // 1 -> 4
  ch.editParam(CP_HITS, 1);  // 1 -> 2
  TEST_ASSERT_EQUAL_INT(4, ch.patternSteps());
  TEST_ASSERT_TRUE(ch.patternHit(0));
  TEST_ASSERT_FALSE(ch.patternHit(1));
  TEST_ASSERT_TRUE(ch.patternHit(2));
  TEST_ASSERT_FALSE(ch.patternHit(3));
}

// The same E(4,2) played out: at the smallest clock division (4 pulses per
// step) the gate rises on ticks 0 and 8 of a 16-tick window. PROB is 100 by
// default, so the ISR path never reaches random().
void test_euclidean_gate_pattern(void) {
  Channel ch;
  ch.setClockMod(MOD_CHOICE_SIZE - 1); // 24 PPQN -> 4 pulses per step
  ch.editParam(CP_STEPS, 3);           // steps 1 -> 4
  ch.editParam(CP_HITS, 1);            // hits 1 -> 2

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

// PROB = 0 silences the channel outright: no gate ever opens, whatever the
// pattern says. This is the branch that replaced the old probability func.
void test_probability_zero_never_fires(void) {
  Channel ch;
  ch.setClockMod(MOD_CHOICE_SIZE - 1);
  ch.editParam(CP_STEPS, 3);
  ch.editParam(CP_HITS, 3); // every step a hit
  ch.editParam(CP_PROB, -100);
  TEST_ASSERT_EQUAL_INT(0, ch.paramValue(CP_PROB, false));

  // random() is overloaded, so the two-arg form has to be named explicitly.
  // 0 is the lowest possible roll: if PROB = 0 still loses to it, it always does.
  When(OverloadedMethod(ArduinoFake(), random, long(long, long)))
      .AlwaysReturn(0);
  DigitalOutput out;
  out.Init(7);
  for (uint32_t t = 0; t < 32; t++) {
    ch.processClockTick(t, out);
    TEST_ASSERT_FALSE(out.On());
  }
}

// A CV routed to one param moves that param's live value only; every other
// param, and the routed param's own base value, stay put.
void test_cv_param_targeting(void) {
  Channel ch;
  ch.editParam(CP_STEPS, 7); // base steps = 8
  TEST_ASSERT_EQUAL_INT(8, ch.paramValue(CP_STEPS, true)); // unrouted -> base

  const uint8_t duty_before = ch.paramValue(CP_DUTY, false);
  ch.setCvDest(0, CV_STEPS); // slot CV1-A
  ch.setCvAmount(0, -100);
  ch.applyCvMod(100, 0); // 100>>3 = 12, so live steps = 8 - 12 -> clamps to 1

  TEST_ASSERT_EQUAL_INT(1, ch.paramValue(CP_STEPS, true));  // routed -> live
  TEST_ASSERT_EQUAL_INT(8, ch.paramValue(CP_STEPS, false)); // base untouched
  TEST_ASSERT_EQUAL_INT(duty_before, ch.paramValue(CP_DUTY, true)); // others
  TEST_ASSERT_EQUAL_INT(1, ch.patternSteps()); // the pattern follows live
}

// Params survive a save/load round trip through the byte record.
void test_param_save_load_roundtrip(void) {
  Channel ch;
  ch.editParam(CP_STEPS, 4); // 5
  ch.editParam(CP_HITS, 2);  // 3
  ch.editParam(CP_ROTATE, 2);
  ch.editParam(CP_PROB, -40);  // 60
  ch.editParam(CP_DUTY, 25);   // 75
  ch.editParam(CP_OFFSET, 10); // 10
  ch.editParam(CP_SWING, 20);  // 70
  ch.setClockMod(MOD_CHOICE_SIZE - 1);

  byte p[Channel::SAVE_BYTES];
  ch.save(p);

  // Move everything, then load it back.
  ch.editParam(CP_STEPS, -3);
  ch.editParam(CP_PROB, 30);
  ch.editParam(CP_DUTY, -20);
  ch.setClockMod(0);
  ch.load(p);

  TEST_ASSERT_EQUAL_INT(5, ch.paramValue(CP_STEPS, false));
  TEST_ASSERT_EQUAL_INT(3, ch.paramValue(CP_HITS, false));
  TEST_ASSERT_EQUAL_INT(2, ch.paramValue(CP_ROTATE, false));
  TEST_ASSERT_EQUAL_INT(60, ch.paramValue(CP_PROB, false));
  TEST_ASSERT_EQUAL_INT(75, ch.paramValue(CP_DUTY, false));
  TEST_ASSERT_EQUAL_INT(10, ch.paramValue(CP_OFFSET, false));
  TEST_ASSERT_EQUAL_INT(70, ch.paramValue(CP_SWING, false));
  TEST_ASSERT_EQUAL_INT(MOD_CHOICE_SIZE - 1, ch.getClockModIndex());
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_defaults);
  RUN_TEST(test_param_ranges);
  RUN_TEST(test_euclidean_pattern_spread);
  RUN_TEST(test_euclidean_gate_pattern);
  RUN_TEST(test_probability_zero_never_fires);
  RUN_TEST(test_cv_param_targeting);
  RUN_TEST(test_param_save_load_roundtrip);
  return UNITY_END();
}
