/**
 * Host unit tests for DigitalOutput (src/digital_output.h).
 *
 * Mocks the Arduino core (pinMode / digitalWrite / millis) via ArduinoFake and
 * verifies the gate/trigger state machine and trigger-duration release timing.
 */
#include <ArduinoFake.h>
#include <unity.h>

#include "digital_output.h"

using namespace fakeit;

void setUp() {
  ArduinoFakeReset();
  When(Method(ArduinoFake(), pinMode)).AlwaysReturn();
  When(Method(ArduinoFake(), digitalWrite)).AlwaysReturn();
}
void tearDown() {}

void test_init_sets_pin_output(void) {
  DigitalOutput out;
  out.Init(7);
  Verify(Method(ArduinoFake(), pinMode).Using(7, OUTPUT)).Once();
}

void test_high_low_toggles_state_and_pin(void) {
  DigitalOutput out;
  out.Init(7);

  out.High();
  TEST_ASSERT_TRUE(out.On());
  out.Low();
  TEST_ASSERT_FALSE(out.On());

  Verify(Method(ArduinoFake(), digitalWrite).Using(7, HIGH)).Once();
  Verify(Method(ArduinoFake(), digitalWrite).Using(7, LOW)).Once();
}

void test_update_maps_state(void) {
  DigitalOutput out;
  out.Init(7);
  out.Update(HIGH);
  TEST_ASSERT_TRUE(out.On());
  out.Update(LOW);
  TEST_ASSERT_FALSE(out.On());
}

void test_trigger_releases_after_duration(void) {
  // millis() call sequence: Trigger()=1000, Process()#1=1002, Process()#2=1006.
  // Default trigger duration is 5 ms, so the output releases on the 2nd Process.
  When(Method(ArduinoFake(), millis)).Return(1000, 1002, 1006);

  DigitalOutput out;
  out.Init(7);

  out.Trigger();
  TEST_ASSERT_TRUE(out.On());

  out.Process();  // 1002 - 1000 = 2ms  (< 5) -> still high
  TEST_ASSERT_TRUE(out.On());

  out.Process();  // 1006 - 1000 = 6ms  (>= 5) -> released
  TEST_ASSERT_FALSE(out.On());
}

void test_custom_trigger_duration(void) {
  When(Method(ArduinoFake(), millis)).Return(0, 20, 60);

  DigitalOutput out;
  out.Init(7);
  out.SetTriggerDuration(50);

  out.Trigger();       // t=0
  out.Process();       // t=20 (< 50) -> high
  TEST_ASSERT_TRUE(out.On());
  out.Process();       // t=60 (>= 50) -> low
  TEST_ASSERT_FALSE(out.On());
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_init_sets_pin_output);
  RUN_TEST(test_high_low_toggles_state_and_pin);
  RUN_TEST(test_update_maps_state);
  RUN_TEST(test_trigger_releases_after_duration);
  RUN_TEST(test_custom_trigger_duration);
  return UNITY_END();
}
