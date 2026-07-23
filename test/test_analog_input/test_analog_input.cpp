/**
 * Host unit tests for AnalogInput (src/analog_input.h).
 *
 * analogRead is backed by a simulated raw value. Covers the ±512 mapping,
 * inversion via attenuation, Voltage(), and IsRisingEdge() - including the
 * bipolar (negative -> positive) crossing that the old uint16_t old_read_ type
 * broke.
 */
#include <ArduinoFake.h>
#include <unity.h>

#include "analog_input.h"

using namespace fakeit;

static int g_raw;  // simulated 10-bit analogRead value

void setUp() {
  ArduinoFakeReset();
  g_raw = 0;
  When(Method(ArduinoFake(), pinMode)).AlwaysReturn();
  When(Method(ArduinoFake(), analogRead)).AlwaysDo([](uint8_t) -> int {
    return g_raw;
  });
  // ArduinoFake mocks map() as a real method (it is a function, not a macro),
  // so give it its genuine Arduino arithmetic instead of a canned return value.
  When(Method(ArduinoFake(), map))
      .AlwaysDo([](long x, long in_min, long in_max, long out_min,
                   long out_max) -> long {
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
      });
}
void tearDown() {}

void test_read_maps_to_bipolar_range(void) {
  AnalogInput ai;
  ai.Init(0);

  g_raw = 1023;
  ai.Process();
  TEST_ASSERT_EQUAL_INT(512, ai.Read());

  g_raw = 0;
  ai.Process();
  TEST_ASSERT_EQUAL_INT(-512, ai.Read());
}

void test_voltage_conversion(void) {
  AnalogInput ai;
  ai.Init(0);
  g_raw = 1023;
  ai.Process();
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 5.0f, ai.Voltage());
}

void test_attenuation_inverts(void) {
  AnalogInput ai;
  ai.Init(0);
  ai.SetAttenuation(-1.0f);  // full range, inverted

  g_raw = 1023;
  ai.Process();
  TEST_ASSERT_EQUAL_INT(-512, ai.Read());
}

// Validates the old_read_ type fix: a rising edge from a negative reading up
// across the 0 threshold must be detected. With the old uint16_t old_read_ the
// stored negative value wrapped high and this returned false.
void test_rising_edge_from_negative(void) {
  AnalogInput ai;
  ai.Init(0);

  g_raw = 0;       // maps to -512
  ai.Process();
  g_raw = 1023;    // maps to +512
  ai.Process();

  TEST_ASSERT_TRUE(ai.IsRisingEdge(0));
}

void test_no_rising_edge_when_already_high(void) {
  AnalogInput ai;
  ai.Init(0);

  g_raw = 1023;
  ai.Process();
  g_raw = 1023;
  ai.Process();

  TEST_ASSERT_FALSE(ai.IsRisingEdge(0));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_read_maps_to_bipolar_range);
  RUN_TEST(test_voltage_conversion);
  RUN_TEST(test_attenuation_inverts);
  RUN_TEST(test_rising_edge_from_negative);
  RUN_TEST(test_no_rising_edge_when_already_high);
  return UNITY_END();
}
