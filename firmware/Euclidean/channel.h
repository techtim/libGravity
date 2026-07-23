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
#include "euclidean.h"

// Enums for CV Mod destination
enum CvDestination : uint8_t {
  CV_DEST_NONE,
  CV_DEST_MOD,
  CV_DEST_EUC_STEPS,
  CV_DEST_EUC_HITS,
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
    base_euc_steps = 1;
    base_euc_hits = 1;

    cvmod_clock_mod_index = base_clock_mod_index;

    cv1_dest = CV_DEST_NONE;
    cv2_dest = CV_DEST_NONE;

    mute = false;

    pattern.Init(DEFAULT_PATTERN);

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

  // Euclidean
  void setSteps(int val) {
    base_euc_steps = constrain(val, 1, MAX_PATTERN_LEN);
    if (cv1_dest != CV_DEST_EUC_STEPS && cv2_dest != CV_DEST_EUC_STEPS) {
      pattern.SetSteps(val);
    }
  }
  void setHits(int val) {
    base_euc_hits = constrain(val, 1, base_euc_steps);
    if (cv1_dest != CV_DEST_EUC_HITS && cv2_dest != CV_DEST_EUC_HITS) {
      pattern.SetHits(val);
    }
  }

  void setCv1Dest(CvDestination dest) { cv1_dest = dest; }
  void setCv2Dest(CvDestination dest) { cv2_dest = dest; }
  CvDestination getCv1Dest() const { return cv1_dest; }
  CvDestination getCv2Dest() const { return cv2_dest; }

  // Getters (Get the BASE value for editing or cv modded value for display)

  int getClockMod(bool withCvMod = false) const {
    return pgm_read_word_near(&CLOCK_MOD[getClockModIndex(withCvMod)]);
  }
  int getClockModIndex(bool withCvMod = false) const {
    return withCvMod ? cvmod_clock_mod_index : base_clock_mod_index;
  }
  bool isCvModActive() const {
    return cv1_dest != CV_DEST_NONE || cv2_dest != CV_DEST_NONE;
  }

  byte getSteps(bool withCvMod = false) const {
    return withCvMod ? pattern.GetSteps() : base_euc_steps;
  }
  byte getHits(bool withCvMod = false) const {
    return withCvMod ? pattern.GetHits() : base_euc_hits;
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

    // Euclidian rhythm cycle check
    if (!output.On()) {
      // Step check
      if (tick % mod_pulses == 0) {
        bool hit = true;
        // Euclidean rhythm hit check
        switch (pattern.NextStep()) {
        case Pattern::REST:
          hit = false;
          break;
        case Pattern::HIT:
          hit &= true;
          break;
        }
        if (hit) {
          output.High();
        }
      }
    }

    // Output low check. Half pulse width.
    const uint32_t duty_cycle_end_tick = tick + _duty_pulses;
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
      return;
    }

    int dest_mod = _calculateMod(CV_DEST_MOD, cv1_val, cv2_val,
                                 -(MOD_CHOICE_SIZE / 2), MOD_CHOICE_SIZE / 2);
    cvmod_clock_mod_index =
        constrain(base_clock_mod_index + dest_mod, 0, MOD_CHOICE_SIZE - 1);

    int step_mod =
        _calculateMod(CV_DEST_EUC_STEPS, cv1_val, cv2_val, 0, MAX_PATTERN_LEN);
    pattern.SetSteps(base_euc_steps + step_mod);

    int hit_mod = _calculateMod(CV_DEST_EUC_HITS, cv1_val, cv2_val, 0,
                                pattern.GetSteps());
    pattern.SetHits(base_euc_hits + hit_mod);

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
    _duty_pulses = max((long)(mod_pulses / 2L), 1L);
  }

  // User-settable base values.
  byte base_clock_mod_index;
  byte base_euc_steps;
  byte base_euc_hits;

  // Base value with cv mod applied.
  byte cvmod_clock_mod_index;

  // CV mod configuration
  CvDestination cv1_dest;
  CvDestination cv2_dest;

  // Euclidean pattern
  Pattern pattern;

  // Mute channel flag
  bool mute;

  // Pre-calculated pulse values for ISR performance
  uint16_t _duty_pulses;
};

#endif // CHANNEL_H