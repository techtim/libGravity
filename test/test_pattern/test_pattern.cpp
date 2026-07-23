/**
 * Host unit tests for the Euclidean Pattern generator (firmware/Euclidean/euclidean.h).
 *
 * Validates the Bresenham-style rhythm generation and the Fix 5 atomic
 * shadow-buffer publish in updatePattern(). No hardware mocking needed - Pattern
 * is pure logic (constrain/min + the util/atomic shim).
 */
#include <ArduinoFake.h>
#include <unity.h>

#include "arduino_compat.h"  // Arduino min()/max() the mock omits; before firmware headers
#include "euclidean.h"

using namespace fakeit;

void setUp() { ArduinoFakeReset(); }
void tearDown() {}

// Collect one full cycle of the pattern into `out` as 0 (REST) / 1 (HIT).
static void collectCycle(Pattern &p, uint8_t *out, int n) {
  p.Reset();
  for (int i = 0; i < n; i++) {
    out[i] = (p.NextStep() == Pattern::HIT) ? 1 : 0;
  }
}

static int countHits(Pattern &p) {
  int hits = 0;
  for (int i = 0; i < p.GetSteps(); i++) {
    if (p.GetCurrentStep(i) == Pattern::HIT) hits++;
  }
  return hits;
}

void test_e4_2_is_1010(void) {
  Pattern p;
  p.Init({4, 2});
  TEST_ASSERT_EQUAL_UINT8(4, p.GetSteps());
  TEST_ASSERT_EQUAL_UINT8(2, p.GetHits());
  uint8_t got[4];
  collectCycle(p, got, 4);
  uint8_t want[4] = {1, 0, 1, 0};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(want, got, 4);
}

void test_e3_8_is_10010010(void) {
  Pattern p;
  p.Init({8, 3});
  uint8_t got[8];
  collectCycle(p, got, 8);
  uint8_t want[8] = {1, 0, 0, 1, 0, 0, 1, 0};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(want, got, 8);
}

void test_e5_16_hit_count_and_downbeat(void) {
  Pattern p;
  p.Init({16, 5});
  TEST_ASSERT_EQUAL_INT(5, countHits(p));
  // Euclidean patterns always place a hit on the downbeat (index 0).
  TEST_ASSERT_EQUAL(Pattern::HIT, p.GetCurrentStep(0));
}

void test_hits_equal_steps_all_on(void) {
  Pattern p;
  p.Init({6, 6});
  TEST_ASSERT_EQUAL_INT(6, countHits(p));
}

void test_single_hit_is_downbeat_only(void) {
  Pattern p;
  p.Init({8, 1});
  TEST_ASSERT_EQUAL_INT(1, countHits(p));
  TEST_ASSERT_EQUAL(Pattern::HIT, p.GetCurrentStep(0));
}

void test_max_pattern_len_boundary(void) {
  // MAX_PATTERN_LEN == 32: exercises bit 31 with no unsigned-long overflow.
  Pattern p;
  p.Init({MAX_PATTERN_LEN, MAX_PATTERN_LEN});
  TEST_ASSERT_EQUAL_UINT8(MAX_PATTERN_LEN, p.GetSteps());
  TEST_ASSERT_EQUAL_INT(MAX_PATTERN_LEN, countHits(p));
  TEST_ASSERT_EQUAL(Pattern::HIT, p.GetCurrentStep(31));
}

void test_init_clamps_out_of_range(void) {
  Pattern p;
  // steps above the max and hits above steps must be clamped, not overflow.
  p.Init({250, 250});
  TEST_ASSERT_EQUAL_UINT8(MAX_PATTERN_LEN, p.GetSteps());
  TEST_ASSERT_TRUE(p.GetHits() <= p.GetSteps());
}

void test_set_steps_clamps_hits_down(void) {
  Pattern p;
  p.Init({8, 8});
  p.SetSteps(3);
  TEST_ASSERT_EQUAL_UINT8(3, p.GetSteps());
  TEST_ASSERT_TRUE(p.GetHits() <= 3);
}

void test_nextstep_wraps_around(void) {
  Pattern p;
  p.Init({4, 2});  // 1010
  Pattern::Step first[4];
  Pattern::Step second[4];
  p.Reset();
  for (int i = 0; i < 4; i++) first[i] = p.NextStep();
  // step_index_ has wrapped back to 0; a second pass must repeat the pattern.
  for (int i = 0; i < 4; i++) second[i] = p.NextStep();
  for (int i = 0; i < 4; i++) TEST_ASSERT_EQUAL(first[i], second[i]);
}

void test_reset_returns_to_index_zero(void) {
  Pattern p;
  p.Init({4, 2});
  p.NextStep();
  p.NextStep();
  TEST_ASSERT_NOT_EQUAL(0, p.GetStepIndex());
  p.Reset();
  TEST_ASSERT_EQUAL_UINT8(0, p.GetStepIndex());
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_e4_2_is_1010);
  RUN_TEST(test_e3_8_is_10010010);
  RUN_TEST(test_e5_16_hit_count_and_downbeat);
  RUN_TEST(test_hits_equal_steps_all_on);
  RUN_TEST(test_single_hit_is_downbeat_only);
  RUN_TEST(test_max_pattern_len_boundary);
  RUN_TEST(test_init_clamps_out_of_range);
  RUN_TEST(test_set_steps_clamps_hits_down);
  RUN_TEST(test_nextstep_wraps_around);
  RUN_TEST(test_reset_returns_to_index_zero);
  return UNITY_END();
}
