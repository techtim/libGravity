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
  // Playback state.
  uint32_t bitmap;
  uint8_t step_index;
  // Phase at which the gate closes (mod_pulses - 50% duty); cached by finalize()
  // so process() doesn't recompute the duty every tick.
  uint16_t low_phase;
  // Persisted (base) params.
  uint8_t base_steps;
  uint8_t base_hits;
  uint8_t base_prob;
  // Param (CV-modulated) values that drive playback.
  uint8_t steps;
  uint8_t hits;
  uint8_t prob;

  void reset() {
    base_steps = 1;
    base_hits = 1;
    base_prob = 100; // hits always fire by default
    step_index = 0;
    syncParam();
    finalize(1);
  }

  // Restart the pattern from the beginning without touching params. Called on a
  // clock (re)start/reset so a RESTART returns the sequence to step 0.
  void resetPlayback() { step_index = 0; }

  static uint8_t paramCount() { return 3; }
  static const __FlashStringHelper *funcName(bool full) {
    return !full ? F("xOx") : F("EUCLIDEAN");
  }
  static const __FlashStringHelper *paramLabel(uint8_t i) {
    switch (i) {
    case 0: return F("STEPS");
    case 1: return F("HITS");
    default: return F("PROB");
    }
  }
  // Bipolar CV maps into these ranges. Hits scales to the current step count.
  void cvRange(uint8_t i, int &lo, int &hi) const {
    switch (i) {
    case 0: lo = 0; hi = MAX_PATTERN_LEN; break;
    case 1: lo = 0; hi = steps; break;
    default: lo = -50; hi = 50; break; // prob
    }
  }

  int getBase(uint8_t i) const {
    switch (i) {
    case 0: return base_steps;
    case 1: return base_hits;
    default: return base_prob;
    }
  }
  void setBase(uint8_t i, int v) {
    switch (i) {
    case 0:
      base_steps = constrain(v, 1, MAX_PATTERN_LEN);
      if (base_hits > base_steps)
        base_hits = base_steps;
      break;
    case 1:
      base_hits = constrain(v, 1, base_steps);
      break;
    default:
      base_prob = constrain(v, 0, 100);
      break;
    }
  }
  int getParam(uint8_t i) const {
    switch (i) {
    case 0: return steps;
    case 1: return hits;
    default: return prob;
    }
  }
  void setParam(uint8_t i, int v) {
    switch (i) {
    case 0:
      steps = constrain(v, 1, MAX_PATTERN_LEN);
      if (hits > steps)
        hits = steps;
      break;
    case 1:
      hits = constrain(v, 1, steps);
      break;
    default:
      prob = constrain(v, 0, 100);
      break;
    }
  }
  void syncParam() {
    steps = base_steps;
    hits = base_hits;
    prob = base_prob;
  }
  // Cache the gate-close phase (mod_pulses - 50% duty) so process() stays a pair
  // of compares. finalize() runs on every param / clock-mod change, so low_phase
  // tracks the current mod_pulses.
  void finalize(uint16_t mod_pulses) {
    regen();
    if (step_index >= steps)
      step_index = 0;
    uint16_t duty = mod_pulses >> 1;
    if (duty == 0)
      duty = 1;
    low_phase = mod_pulses - duty;
  }

  void save(byte *p) const {
    p[0] = base_steps;
    p[1] = base_hits;
    p[2] = base_prob;
  }
  void load(const byte *p) {
    base_steps = constrain((int)p[0], 1, MAX_PATTERN_LEN);
    base_hits = constrain((int)p[1], 1, base_steps);
    base_prob = constrain((int)p[2], 0, 100);
  }

  // ISR: advance the euclidean pattern; a HIT only fires if the probability
  // roll passes. prob == 100 short-circuits the RNG (the common case). Fixed
  // 50% duty gate.
  void process(const StepContext &ctx) {
    if (!ctx.output.On()) {
      if (ctx.phase == 0) {
        if (nextStep() && (prob >= 100 || prob > (uint8_t)random(0, 100)))
          ctx.output.High();
      }
    }
    // Gate closes at the cached 50%-duty phase (precomputed in finalize()).
    if (ctx.phase == low_phase)
      ctx.output.Low();
  }

private:
  bool nextStep() {
    bool hit = (bitmap & (1UL << step_index)) != 0;
    step_index = (step_index < steps - 1) ? step_index + 1 : 0;
    return hit;
  }
  void regen() {
    bitmap = 0;
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
