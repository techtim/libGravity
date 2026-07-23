/**
 * Host unit tests for the Euclidean Channel (firmware/Euclidean/channel.h).
 *
 * Requires channel.h to include only "digital_output.h" (not the full
 * <libGravity.h> umbrella) so it compiles on the host. Covers clock-mod / steps
 * / hits clamping, the Fix 2 CV-mod index bound, and mute behavior.
 */
#include <ArduinoFake.h>
#include <unity.h>

#include "arduino_compat.h"  // Arduino min()/max() the mock omits; before firmware headers
#include "channel.h"

using namespace fakeit;

void setUp() {
  ArduinoFakeReset();
  When(Method(ArduinoFake(), pinMode)).AlwaysReturn();
  When(Method(ArduinoFake(), digitalWrite)).AlwaysReturn();
  // ArduinoFake mocks map() as a real method (it is a function, not a macro),
  // so give it its genuine Arduino arithmetic (applyCvMod relies on it).
  When(Method(ArduinoFake(), map))
      .AlwaysDo([](long x, long in_min, long in_max, long out_min,
                   long out_max) -> long {
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
      });
}
void tearDown() {}

void test_default_clock_mod_is_unity(void) {
  Channel ch;
  TEST_ASSERT_EQUAL_INT(DEFAULT_CLOCK_MOD_INDEX, ch.getClockModIndex(false));
  // CLOCK_MOD[DEFAULT_CLOCK_MOD_INDEX] is quarter-note unity (x1).
  TEST_ASSERT_EQUAL_INT(1, ch.getClockMod(false));
}

void test_set_clock_mod_clamps(void) {
  Channel ch;
  ch.setClockMod(999);
  TEST_ASSERT_EQUAL_INT(MOD_CHOICE_SIZE - 1, ch.getClockModIndex(false));
  ch.setClockMod(-5);
  TEST_ASSERT_EQUAL_INT(0, ch.getClockModIndex(false));
}

void test_set_steps_and_hits_clamp(void) {
  Channel ch;
  ch.setSteps(999);
  TEST_ASSERT_EQUAL_UINT8(MAX_PATTERN_LEN, ch.getSteps(false));
  ch.setSteps(0);
  TEST_ASSERT_EQUAL_UINT8(1, ch.getSteps(false));

  ch.setSteps(4);
  ch.setHits(999);
  TEST_ASSERT_TRUE(ch.getHits(false) <= ch.getSteps(false));
}

// Regression guard for Fix 2: CV modulation of the clock mod must never push
// cvmod_clock_mod_index outside the CLOCK_MOD_PULSES[] bounds (was clamped to
// 100, causing an out-of-bounds PROGMEM read / modulo-by-zero in the ISR).
void test_cvmod_clock_index_stays_in_bounds(void) {
  Channel ch;
  ch.setClockMod(MOD_CHOICE_SIZE - 1);  // base already at max
  ch.setCv1Dest(CV_DEST_MOD);
  ch.applyCvMod(512, 0);  // full-scale positive CV -> largest positive mod

  int idx = ch.getClockModIndex(true);
  TEST_ASSERT_GREATER_OR_EQUAL_INT(0, idx);
  TEST_ASSERT_LESS_OR_EQUAL_INT(MOD_CHOICE_SIZE - 1, idx);

  // And with base at the bottom + full-scale negative CV.
  ch.setClockMod(0);
  ch.applyCvMod(-512, 0);
  idx = ch.getClockModIndex(true);
  TEST_ASSERT_GREATER_OR_EQUAL_INT(0, idx);
  TEST_ASSERT_LESS_OR_EQUAL_INT(MOD_CHOICE_SIZE - 1, idx);
}

void test_no_cvmod_uses_base(void) {
  Channel ch;
  ch.setClockMod(5);
  // No CV destination configured -> cvmod value tracks base.
  TEST_ASSERT_FALSE(ch.isCvModActive());
  TEST_ASSERT_EQUAL_INT(5, ch.getClockModIndex(true));
}

void test_mute_forces_output_low(void) {
  DigitalOutput out;
  out.Init(7);
  out.High();
  TEST_ASSERT_TRUE(out.On());

  Channel ch;
  ch.toggleMute();               // now muted
  ch.processClockTick(0, out);   // muted -> forces Low regardless of tick
  TEST_ASSERT_FALSE(out.On());
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_default_clock_mod_is_unity);
  RUN_TEST(test_set_clock_mod_clamps);
  RUN_TEST(test_set_steps_and_hits_clamp);
  RUN_TEST(test_cvmod_clock_index_stays_in_bounds);
  RUN_TEST(test_no_cvmod_uses_base);
  RUN_TEST(test_mute_forces_output_low);
  return UNITY_END();
}
