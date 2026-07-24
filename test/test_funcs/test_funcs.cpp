/**
 * Host unit tests for the Modal firmware's unified Channel + funcs.
 *
 * Uses a relative include for the entry header: firmware/Euclidean is already on
 * the native -I path and shares the "channel.h" filename, so we point explicitly
 * at the Modal one. Its own same-directory quote-includes resolve the rest.
 */
#include <ArduinoFake.h>
#include <unity.h>

#include "arduino_compat.h" // Arduino min()/max() the mock omits
#include "../../firmware/Modal/channel.h"

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

void test_default_func_and_switching(void) {
  Channel ch;
  TEST_ASSERT_EQUAL(FUNC_PROBABILITY, ch.getFunc());
  TEST_ASSERT_EQUAL_UINT8(4, ch.paramCount());
  ch.setFunc(FUNC_EUCLIDEAN);
  TEST_ASSERT_EQUAL(FUNC_EUCLIDEAN, ch.getFunc());
  TEST_ASSERT_EQUAL_UINT8(2, ch.paramCount());
}

// Euclidean E(4,2) at the smallest clock division (mod_pulses = 4) fires on
// ticks 0 and 8 across one 16-tick window.
void test_euclidean_gate_pattern(void) {
  Channel ch;
  ch.setFunc(FUNC_EUCLIDEAN);
  ch.setClockMod(MOD_CHOICE_SIZE - 1);
  ch.editParam(0, 3); // steps 1 -> 4
  ch.editParam(1, 1); // hits 1 -> 2

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

// Probability param model + persistence. (Its ISR firing calls random(), which
// is covered by the offline harness and the shipping Gravity firmware; kept out
// of this suite to avoid mocking an overloaded function.)
void test_probability_params(void) {
  Channel ch; // default PROBABILITY
  TEST_ASSERT_EQUAL_UINT8(4, ch.paramCount());
  TEST_ASSERT_EQUAL_INT(100, ch.paramValue(0, false)); // prob default
  TEST_ASSERT_EQUAL_INT(50, ch.paramValue(1, false));  // duty default

  ch.editParam(0, -30); // prob -> 70
  TEST_ASSERT_EQUAL_INT(70, ch.paramValue(0, false));

  byte payload[FUNC_PAYLOAD_MAX] = {0};
  ch.saveFunc(payload);
  ch.editParam(0, 20); // prob -> 90
  ch.loadFunc(payload);
  TEST_ASSERT_EQUAL_INT(70, ch.paramValue(0, false)); // restored
}

// CV routed to a func param modulates only that param; the display getter shows
// param for the routed param and base for the others.
void test_cv_param_targeting(void) {
  Channel ch;
  ch.setFunc(FUNC_EUCLIDEAN);
  ch.editParam(0, 3); // base steps = 4
  TEST_ASSERT_EQUAL_INT(4, ch.paramValue(0, true)); // not routed -> base

  ch.setCv1Target(CV_PARAM_0);
  ch.applyCvMod(512, 0); // steps 4 + map(512,-512,512,0,32)=32 -> clamp 32
  TEST_ASSERT_EQUAL_INT(32, ch.paramValue(0, true));  // routed -> param
  TEST_ASSERT_EQUAL_INT(4, ch.paramValue(0, false));  // base
  TEST_ASSERT_EQUAL_INT(ch.paramValue(1, false), ch.paramValue(1, true));
}

void test_save_load_roundtrip(void) {
  Channel ch;
  ch.setFunc(FUNC_EUCLIDEAN);
  ch.editParam(0, 4); // steps 5
  ch.editParam(1, 2); // hits 3
  byte payload[FUNC_PAYLOAD_MAX] = {0};
  ch.saveFunc(payload);
  ch.editParam(0, -3);
  ch.editParam(1, -1);
  ch.loadFunc(payload);
  TEST_ASSERT_EQUAL_INT(5, ch.paramValue(0, false));
  TEST_ASSERT_EQUAL_INT(3, ch.paramValue(1, false));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_default_func_and_switching);
  RUN_TEST(test_euclidean_gate_pattern);
  RUN_TEST(test_probability_params);
  RUN_TEST(test_cv_param_targeting);
  RUN_TEST(test_save_load_roundtrip);
  return UNITY_END();
}
