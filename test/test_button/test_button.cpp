/**
 * Host unit tests for Button (src/button.h).
 *
 * digitalRead / millis are backed by module-level "simulated" state so tests can
 * drive logical pin level and time without counting individual mock calls.
 * Button uses INPUT_PULLUP semantics: 0 == pressed, 1 == released.
 */
#include <ArduinoFake.h>
#include <unity.h>

#include "button.h"

using namespace fakeit;

static int g_pin_level;             // 0 = pressed, 1 = released
static unsigned long g_now_ms;
static int g_press_count;
static int g_long_press_count;

static void onPress() { g_press_count++; }
static void onLongPress() { g_long_press_count++; }

void setUp() {
  ArduinoFakeReset();
  g_pin_level = 1;  // released
  g_now_ms = 0;
  g_press_count = 0;
  g_long_press_count = 0;
  When(Method(ArduinoFake(), pinMode)).AlwaysReturn();
  When(Method(ArduinoFake(), digitalRead)).AlwaysDo([](uint8_t) -> int {
    return g_pin_level;
  });
  When(Method(ArduinoFake(), millis)).AlwaysDo([]() -> unsigned long {
    return g_now_ms;
  });
}
void tearDown() {}

static Button makeButton() {
  Button b;
  b.Init(5);
  b.AttachPressHandler(onPress);
  b.AttachLongPressHandler(onLongPress);
  return b;
}

void test_press_then_short_release(void) {
  Button b = makeButton();

  g_pin_level = 0;
  g_now_ms = 100;
  b.Process();
  TEST_ASSERT_EQUAL(Button::CHANGE_PRESSED, b.Change());

  g_pin_level = 1;
  g_now_ms = 200;  // 100ms held (< 750ms long-press threshold)
  b.Process();
  TEST_ASSERT_EQUAL(Button::CHANGE_RELEASED, b.Change());
  TEST_ASSERT_EQUAL_INT(1, g_press_count);
  TEST_ASSERT_EQUAL_INT(0, g_long_press_count);
}

void test_long_press_release(void) {
  Button b = makeButton();

  g_pin_level = 0;
  g_now_ms = 100;
  b.Process();  // pressed

  g_pin_level = 1;
  g_now_ms = 1000;  // 900ms held (> 750ms) -> long press
  b.Process();
  TEST_ASSERT_EQUAL(Button::CHANGE_RELEASED_LONG, b.Change());
  TEST_ASSERT_EQUAL_INT(0, g_press_count);
  TEST_ASSERT_EQUAL_INT(1, g_long_press_count);
}

void test_debounce_ignores_fast_bounce(void) {
  Button b = makeButton();

  g_pin_level = 0;
  g_now_ms = 100;
  b.Process();  // pressed, last_press_ = 100
  TEST_ASSERT_EQUAL(Button::CHANGE_PRESSED, b.Change());

  g_pin_level = 1;
  g_now_ms = 105;  // only 5ms later (< 10ms debounce) -> ignored
  b.Process();
  TEST_ASSERT_EQUAL(Button::CHANGE_UNCHANGED, b.Change());
  TEST_ASSERT_EQUAL_INT(0, g_press_count);
}

void test_on_reflects_level(void) {
  Button b = makeButton();
  g_pin_level = 0;
  TEST_ASSERT_TRUE(b.On());
  g_pin_level = 1;
  TEST_ASSERT_FALSE(b.On());
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_press_then_short_release);
  RUN_TEST(test_long_press_release);
  RUN_TEST(test_debounce_ignores_fast_bounce);
  RUN_TEST(test_on_reflects_level);
  return UNITY_END();
}
