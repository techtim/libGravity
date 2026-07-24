/**
 * @file func_probability.h
 * @brief Probability channel func (prob / duty / offset / swing). POD state.
 *
 * Ports firmware/Gravity/channel.h's processClockTick + pulse precompute. The
 * duty/offset/swing pulse counts depend on the channel's clock-mod pulses, so
 * finalize() takes mod_pulses and precomputes them (keeps the ISR division-free).
 */
#ifndef MODAL_FUNC_PROBABILITY_H
#define MODAL_FUNC_PROBABILITY_H

#include <Arduino.h>

#include "func_common.h"

struct ProbabilityState {
  // Persisted (base) params.
  uint8_t base_prob;
  uint8_t base_duty;
  uint8_t base_offset;
  uint8_t base_swing;
  // Param (CV-modulated) params.
  uint8_t prob;
  uint8_t duty;
  uint8_t offset;
  uint8_t swing;
  // Precomputed pulse counts (from finalize()).
  uint16_t _duty_pulses;
  uint16_t _offset_pulses;
  uint16_t _swing_pulse_amount;

  void reset() {
    base_prob = 100;
    base_duty = 50;
    base_offset = 0;
    base_swing = 50;
    syncParam();
    finalize(1);
  }

  static uint8_t paramCount() { return 4; }
  static const __FlashStringHelper *funcName(bool full) { return !full ? F("PR") : F("PROBABILITY"); }
  static const __FlashStringHelper *paramLabel(uint8_t i) {
    switch (i) {
    case 0: return F("PROB");
    case 1: return F("DUTY");
    case 2: return F("OFFSET");
    default: return F("SWING");
    }
  }
  void cvRange(uint8_t i, int &lo, int &hi) const {
    if (i == 3) { lo = -25; hi = 25; } // swing
    else { lo = -50; hi = 50; }
  }

  int getBase(uint8_t i) const {
    switch (i) {
    case 0: return base_prob;
    case 1: return base_duty;
    case 2: return base_offset;
    default: return base_swing;
    }
  }
  void setBase(uint8_t i, int v) {
    switch (i) {
    case 0: base_prob = constrain(v, 0, 100); break;
    case 1: base_duty = constrain(v, 1, 99); break;
    case 2: base_offset = constrain(v, 0, 99); break;
    default: base_swing = constrain(v, 50, 95); break;
    }
  }
  int getParam(uint8_t i) const {
    switch (i) {
    case 0: return prob;
    case 1: return duty;
    case 2: return offset;
    default: return swing;
    }
  }
  void setParam(uint8_t i, int v) {
    switch (i) {
    case 0: prob = constrain(v, 0, 100); break;
    case 1: duty = constrain(v, 1, 99); break;
    case 2: offset = constrain(v, 0, 99); break;
    default: swing = constrain(v, 50, 95); break;
    }
  }
  void syncParam() {
    prob = base_prob;
    duty = base_duty;
    offset = base_offset;
    swing = base_swing;
  }
  void finalize(uint16_t mod_pulses) {
    _duty_pulses = max((long)((mod_pulses * (100L - duty)) / 100L), 1L);
    _offset_pulses = (uint16_t)((mod_pulses * (100L - offset)) / 100L);
    if (swing > 50) {
      int shifted = swing - 50;
      _swing_pulse_amount = (uint16_t)((mod_pulses * (100L - shifted)) / 100L);
    } else {
      _swing_pulse_amount = 0;
    }
  }

  void save(byte *p) const {
    p[0] = base_prob;
    p[1] = base_duty;
    p[2] = base_offset;
    p[3] = base_swing;
  }
  void load(const byte *p) {
    base_prob = constrain((int)p[0], 0, 100);
    base_duty = constrain((int)p[1], 1, 99);
    base_offset = constrain((int)p[2], 0, 99);
    base_swing = constrain((int)p[3], 50, 95);
  }

  // ISR: probabilistic gate with duty / offset / swing.
  void process(const StepContext &ctx) {
    uint16_t swing_pulses = 0;
    if (_swing_pulse_amount > 0 && (ctx.tick / ctx.mod_pulses) % 2 == 1)
      swing_pulses = _swing_pulse_amount;

    const uint32_t high_tick = ctx.tick + _offset_pulses + swing_pulses;
    if (!ctx.output.On()) {
      if (high_tick % ctx.mod_pulses == 0) {
        if (prob >= random(0, 100))
          ctx.output.High();
      }
    }
    const uint32_t low_tick =
        ctx.tick + _duty_pulses + _offset_pulses + swing_pulses;
    if (low_tick % ctx.mod_pulses == 0)
      ctx.output.Low();
  }
};

#endif // MODAL_FUNC_PROBABILITY_H
