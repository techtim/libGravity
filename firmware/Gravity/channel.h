/**
 * @file channel.h
 * @author Adam Wonak (https://github.com/awonak/)
 * @brief Alt firmware version of Gravity by Sitka Instruments.
 * @version 2.0.2
 * @date 2025-07-04
 *
 * @copyright MIT - (c) 2025 - Adam Wonak - adam.wonak@gmail.com
 *
 */

#ifndef CHANNEL_H
#define CHANNEL_H

#include <Arduino.h>
#include "digital_output.h"

// Enums for CV Mod destination
enum CvDestination : uint8_t {
  CV_DEST_NONE,
  CV_DEST_MOD,
  CV_DEST_PROB,
  CV_DEST_DUTY,
  CV_DEST_OFFSET,
  CV_DEST_SWING,
  CV_DEST_LAST,
};

static const byte MOD_CHOICE_SIZE = 25;

// Negative numbers are multipliers, positive are divisors.
static const int CLOCK_MOD[MOD_CHOICE_SIZE] PROGMEM = {
    // Divisors
    128, 64, 32, 24, 16, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2,
    // Internal Clock Unity (quarter note)
    1,
    // Multipliers
    -2, -3, -4, -6, -8, -12, -16, -24};

// This represents the number of clock pulses for a 96 PPQN clock source
// that match the above div/mult mods.
static const int CLOCK_MOD_PULSES[MOD_CHOICE_SIZE] PROGMEM = {
    // Divisor Pulses (96 * X)
    12288, 6144, 3072, 2304, 1536, 1152, 1056, 960, 864, 768, 672, 576, 480,
    384, 288, 192,
    // Internal Clock Pulses
    96,
    // Multiplier Pulses (96 / X)
    48, 32, 24, 16, 12, 8, 6, 4};

static const byte DEFAULT_CLOCK_MOD_INDEX = 16; // x1 or 96 PPQN.

static const byte PULSE_PPQN_24_CLOCK_MOD_INDEX = MOD_CHOICE_SIZE - 1;
static const byte PULSE_PPQN_4_CLOCK_MOD_INDEX = MOD_CHOICE_SIZE - 6;
static const byte PULSE_PPQN_1_CLOCK_MOD_INDEX = MOD_CHOICE_SIZE - 9;

class Channel {
public:
  Channel() { Init(); }

  void Init() {
    // Reset base values to their defaults
    base_clock_mod_index = DEFAULT_CLOCK_MOD_INDEX;
    base_probability = 100;
    base_duty_cycle = 50;
    base_offset = 0;
    base_swing = 50;

    cvmod_clock_mod_index = base_clock_mod_index;
    cvmod_probability = base_probability;
    cvmod_duty_cycle = base_duty_cycle;
    cvmod_offset = base_offset;
    cvmod_swing = base_swing;

    cv1_dest = CV_DEST_NONE;
    cv2_dest = CV_DEST_NONE;

    mute = false;

    // Calcule the clock mod pulses on init.
    _recalculatePulses();
  }

  // Setters (Set the BASE value)

  void setClockMod(int index) {
    base_clock_mod_index = constrain(index, 0, MOD_CHOICE_SIZE - 1);
    if (!isCvModActive()) {
      cvmod_clock_mod_index = base_clock_mod_index;
      _recalculatePulses();
    }
  }

  void setProbability(int prob) {
    base_probability = constrain(prob, 0, 100);
    if (!isCvModActive()) {
      cvmod_probability = base_probability;
      _recalculatePulses();
    }
  }

  void setDutyCycle(int duty) {
    base_duty_cycle = constrain(duty, 1, 99);
    if (!isCvModActive()) {
      cvmod_duty_cycle = base_duty_cycle;
      _recalculatePulses();
    }
  }

  void setOffset(int off) {
    base_offset = constrain(off, 0, 99);
    if (!isCvModActive()) {
      cvmod_offset = base_offset;
      _recalculatePulses();
    }
  }
  void setSwing(int val) {
    base_swing = constrain(val, 50, 95);
    if (!isCvModActive()) {
      cvmod_swing = base_swing;
      _recalculatePulses();
    }
  }

  void setCv1Dest(CvDestination dest) { cv1_dest = dest; }
  void setCv2Dest(CvDestination dest) { cv2_dest = dest; }
  CvDestination getCv1Dest() const { return cv1_dest; }
  CvDestination getCv2Dest() const { return cv2_dest; }

  // Getters (Get the BASE value for editing or cv modded value for display)

  int getProbability(bool withCvMod = false) const {
    return withCvMod ? cvmod_probability : base_probability;
  }
  int getDutyCycle(bool withCvMod = false) const {
    return withCvMod ? cvmod_duty_cycle : base_duty_cycle;
  }
  int getOffset(bool withCvMod = false) const {
    return withCvMod ? cvmod_offset : base_offset;
  }
  int getSwing(bool withCvMod = false) const {
    return withCvMod ? cvmod_swing : base_swing;
  }
  int getClockMod(bool withCvMod = false) const {
    return pgm_read_word_near(&CLOCK_MOD[getClockModIndex(withCvMod)]);
  }
  int getClockModIndex(bool withCvMod = false) const {
    return withCvMod ? cvmod_clock_mod_index : base_clock_mod_index;
  }
  bool isCvModActive() const {
    return cv1_dest != CV_DEST_NONE || cv2_dest != CV_DEST_NONE;
  }

  void toggleMute() { mute = !mute; }

  /**
   * @brief Processes a clock tick and determines if the output should be high
   * or low. Note: this method is called from an ISR and must be kept as simple
   * as possible.
   * @param tick The current clock tick count.
   * @param output The output object to be modified.
   */
  void processClockTick(uint32_t tick, DigitalOutput &output) {
    // Mute check
    if (mute) {
      output.Low();
      return;
    }

    const uint16_t mod_pulses =
        pgm_read_word_near(&CLOCK_MOD_PULSES[cvmod_clock_mod_index]);

    // Conditionally apply swing on down beats.
    uint16_t swing_pulses = 0;
    if (_swing_pulse_amount > 0 && (tick / mod_pulses) % 2 == 1) {
      swing_pulses = _swing_pulse_amount;
    }

    // Duty cycle high check logic
    const uint32_t current_tick_offset = tick + _offset_pulses + swing_pulses;
    if (!output.On()) {
      // Step check
      if (current_tick_offset % mod_pulses == 0) {
        bool hit = cvmod_probability >= random(0, 100);
        if (hit) {
          output.High();
        }
      }
    }

    // Duty cycle low check
    const uint32_t duty_cycle_end_tick =
        tick + _duty_pulses + _offset_pulses + swing_pulses;
    if (duty_cycle_end_tick % mod_pulses == 0) {
      output.Low();
    }
  }
  /**
   * @brief Calculate and store cv modded values using bipolar mapping.
   * Default to base value if not the current CV destination.
   *
   * @param cv1_val analog input reading for cv1
   * @param cv2_val analog input reading for cv2
   *
   */
  void applyCvMod(int cv1_val, int cv2_val) {
    // Note: This is optimized for cpu performance. This method is called
    // from the main loop and stores the cv mod values. This reduces CPU
    // cycles inside the internal clock interrupt, which is preferrable.
    // However, if RAM usage grows too much, we have an opportunity to
    // refactor this to store just the CV read values, and calculate the
    // cv mod value per channel inside the getter methods by passing cv
    // values. This would reduce RAM usage, but would introduce a
    // significant CPU cost, which may have undesirable performance issues.
    if (!isCvModActive()) {
      cvmod_clock_mod_index = base_clock_mod_index;
      cvmod_probability = base_clock_mod_index;
      cvmod_duty_cycle = base_clock_mod_index;
      cvmod_offset = base_clock_mod_index;
      cvmod_swing = base_clock_mod_index;
      return;
    }

    int dest_mod = _calculateMod(CV_DEST_MOD, cv1_val, cv2_val,
                                 -(MOD_CHOICE_SIZE / 2), MOD_CHOICE_SIZE / 2);
    cvmod_clock_mod_index =
        constrain(base_clock_mod_index + dest_mod, 0, MOD_CHOICE_SIZE - 1);

    int prob_mod = _calculateMod(CV_DEST_PROB, cv1_val, cv2_val, -50, 50);
    cvmod_probability = constrain(base_probability + prob_mod, 0, 100);

    int duty_mod = _calculateMod(CV_DEST_DUTY, cv1_val, cv2_val, -50, 50);
    cvmod_duty_cycle = constrain(base_duty_cycle + duty_mod, 1, 99);

    int offset_mod = _calculateMod(CV_DEST_OFFSET, cv1_val, cv2_val, -50, 50);
    cvmod_offset = constrain(base_offset + offset_mod, 0, 99);

    int swing_mod = _calculateMod(CV_DEST_SWING, cv1_val, cv2_val, -25, 25);
    cvmod_swing = constrain(base_swing + swing_mod, 50, 95);

    // After all cvmod values are updated, recalculate clock pulse modifiers.
    _recalculatePulses();
  }

private:
  int _calculateMod(CvDestination dest, int cv1_val, int cv2_val, int min_range,
                    int max_range) {
    int mod1 =
        (cv1_dest == dest) ? map(cv1_val, -512, 512, min_range, max_range) : 0;
    int mod2 =
        (cv2_dest == dest) ? map(cv2_val, -512, 512, min_range, max_range) : 0;
    return mod1 + mod2;
  }

  void _recalculatePulses() {
    const uint16_t mod_pulses =
        pgm_read_word_near(&CLOCK_MOD_PULSES[cvmod_clock_mod_index]);
    _duty_pulses =
        max((long)((mod_pulses * (100L - cvmod_duty_cycle)) / 100L), 1L);
    _offset_pulses = (long)((mod_pulses * (100L - cvmod_offset)) / 100L);

    // Calculate the down beat swing amount.
    if (cvmod_swing > 50) {
      int shifted_swing = cvmod_swing - 50;
      _swing_pulse_amount =
          (long)((mod_pulses * (100L - shifted_swing)) / 100L);
    } else {
      _swing_pulse_amount = 0;
    }
  }

  // User-settable base values.
  byte base_clock_mod_index;
  byte base_probability;
  byte base_duty_cycle;
  byte base_offset;
  byte base_swing;

  // Base value with cv mod applied.
  byte cvmod_clock_mod_index;
  byte cvmod_probability;
  byte cvmod_duty_cycle;
  byte cvmod_offset;
  byte cvmod_swing;

  // CV mod configuration
  CvDestination cv1_dest;
  CvDestination cv2_dest;

  // Mute channel flag
  bool mute;

  // Pre-calculated pulse values for ISR performance
  uint16_t _duty_pulses;
  uint16_t _offset_pulses;
  uint16_t _swing_pulse_amount;
};

#endif // CHANNEL_H