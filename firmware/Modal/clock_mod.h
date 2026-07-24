/**
 * @file clock_mod.h
 * @brief Shared clock-mod tables and CV helpers for the Modal firmware.
 *
 * Extracted from the per-firmware channel.h files so the unified Channel and
 * every func share one definition. Header-only, depends only on the Arduino
 * core (constrain / map / pgm_read_word_near).
 */
#ifndef MODAL_CLOCK_MOD_H
#define MODAL_CLOCK_MOD_H

#include <Arduino.h>

static const byte MOD_CHOICE_SIZE = 25;

// Negative numbers are multipliers, positive are divisors.
static const int CLOCK_MOD[MOD_CHOICE_SIZE] PROGMEM = {
    // Divisors
    128, 64, 32, 24, 16, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2,
    // Internal Clock Unity (quarter note)
    1,
    // Multipliers
    -2, -3, -4, -6, -8, -12, -16, -24};

// Number of 96 PPQN clock pulses that match the div/mult mods above.
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

// Number of 96 PPQN pulses for the clock mod at the given index.
inline uint16_t clockModPulses(byte index) {
  return pgm_read_word_near(&CLOCK_MOD_PULSES[index]);
}

// Human-facing clock mod value (positive = divide, negative = multiply).
inline int clockModValue(byte index) {
  return (int)pgm_read_word_near(&CLOCK_MOD[index]);
}

// Map a bipolar CV reading (-512..512) into [min_range, max_range]. Funcs use
// this to convert a routed CV input into a parameter offset.
inline int bipolarMod(int cv_val, int min_range, int max_range) {
  return map(cv_val, -512, 512, min_range, max_range);
}

#endif // MODAL_CLOCK_MOD_H
