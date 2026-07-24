/**
 * @file func_euclidean.h
 * @brief Euclidean rhythm channel func (steps / hits). POD state for the union.
 *
 * Ports the Bresenham pattern algorithm from firmware/Euclidean/euclidean.h into
 * a trivially-constructible struct so it can live in the FuncState union. Fixed
 * 50% gate duty.
 */
#ifndef MODAL_FUNC_EUCLIDEAN_H
#define MODAL_FUNC_EUCLIDEAN_H

#include <Arduino.h>

#include "func_common.h"

#define MAX_PATTERN_LEN 32

struct EuclideanState {
  // Persisted (base) params.
  uint8_t base_steps;
  uint8_t base_hits;
  // Param (CV-modulated) params that drive playback.
  uint8_t steps;
  uint8_t hits;
  // Playback state.
  uint8_t step_index;
  uint32_t bitmap;

  void reset() {
    base_steps = 1;
    base_hits = 1;
    step_index = 0;
    syncParam();
    finalize(1);
  }

  static uint8_t paramCount() { return 2; }
  static const __FlashStringHelper *funcName(bool full) { return !full ? F("EUC") : F("EUCLIDEAN"); }
  static const __FlashStringHelper *paramLabel(uint8_t i) {
    return (i == 0) ? F("STEPS") : F("HITS");
  }
  // Bipolar CV maps into these ranges. Hits scales to the current step count.
  void cvRange(uint8_t i, int &lo, int &hi) const {
    lo = 0;
    hi = (i == 0) ? MAX_PATTERN_LEN : steps;
  }

  int getBase(uint8_t i) const { return (i == 0) ? base_steps : base_hits; }
  void setBase(uint8_t i, int v) {
    if (i == 0) {
      base_steps = constrain(v, 1, MAX_PATTERN_LEN);
      if (base_hits > base_steps)
        base_hits = base_steps;
    } else {
      base_hits = constrain(v, 1, base_steps);
    }
  }
  int getParam(uint8_t i) const { return (i == 0) ? steps : hits; }
  void setParam(uint8_t i, int v) {
    if (i == 0) {
      steps = constrain(v, 1, MAX_PATTERN_LEN);
      if (hits > steps)
        hits = steps;
    } else {
      hits = constrain(v, 1, steps);
    }
  }
  void syncParam() {
    steps = base_steps;
    hits = base_hits;
  }
  // mod_pulses is unused: euclidean derives its 50% duty inline in process().
  void finalize(uint16_t /*mod_pulses*/) {
    regen();
    if (step_index >= steps)
      step_index = 0;
  }

  void save(byte *p) const {
    p[0] = base_steps;
    p[1] = base_hits;
  }
  void load(const byte *p) {
    base_steps = constrain((int)p[0], 1, MAX_PATTERN_LEN);
    base_hits = constrain((int)p[1], 1, base_steps);
  }

  // ISR: advance the euclidean pattern with a fixed 50% duty gate.
  void process(const StepContext &ctx) {
    if (!ctx.output.On()) {
      if (ctx.tick % ctx.mod_pulses == 0) {
        if (nextStep())
          ctx.output.High();
      }
    }
    uint16_t duty = ctx.mod_pulses >> 1;
    if (duty == 0)
      duty = 1;
    if ((ctx.tick + duty) % ctx.mod_pulses == 0)
      ctx.output.Low();
  }

private:
  bool nextStep() {
    if (steps == 0)
      return false;
    bool hit = (bitmap & (1UL << step_index)) != 0;
    step_index = (step_index < steps - 1) ? step_index + 1 : 0;
    return hit;
  }
  void regen() {
    bitmap = 0;
    if (steps == 0)
      return;
    byte bucket = 0;
    bitmap |= (1UL << 0);
    for (int i = 1; i < steps; i++) {
      bucket += hits;
      if (bucket >= steps) {
        bucket -= steps;
        bitmap |= (1UL << i);
      }
    }
  }
};

#endif // MODAL_FUNC_EUCLIDEAN_H
