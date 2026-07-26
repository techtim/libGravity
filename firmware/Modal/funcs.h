/**
 * @file funcs.h
 * @brief Channel func registry: the enum, the tagged-union state, and the CV
 *        target space. Adding a func is localized to FUNC_LIST + a header.
 *
 * To add a func:
 *   1. Write funcs/func_<x>.h with an <X>State POD implementing the func concept
 *      (see func_common.h) and include it below.
 *   2. Add one X(...) line to FUNC_LIST.
 *   3. Add the enum value to ChannelFunc (before FUNC_LAST).
 * The channel's switch dispatch is generated from FUNC_LIST, so no other file
 * needs editing (except the EEPROM payload budget if the new func is larger).
 */
#ifndef MODAL_FUNCS_H
#define MODAL_FUNCS_H

#include <Arduino.h>

#include "funcs/func_euclidean.h"
#include "funcs/func_probability.h"

enum ChannelFunc : uint8_t {
  FUNC_PROBABILITY,
  FUNC_EUCLIDEAN,
  FUNC_LAST,
};

// The single source of truth for the func set: (enum, union member, state type).
#define FUNC_LIST(X)                                                            \
  X(FUNC_PROBABILITY, prob, ProbabilityState)                                   \
  X(FUNC_EUCLIDEAN, euc, EuclideanState)

// Per-channel runtime state. Trivially constructible (all members are PODs); the
// Channel selects and reset()s the active member.
union FuncState {
#define X(E, M, T) T M;
  FUNC_LIST(X)
#undef X
};

// CV routing target. CV_PARAM_i selects the i-th parameter of the channel's
// current func; CV_CLOCK_MOD targets the shared clock division.
enum CvTarget : uint8_t {
  CV_NONE,
  CV_CLOCK_MOD,
  CV_PARAM_0,
  CV_PARAM_1,
  CV_PARAM_2,
  CV_PARAM_3,
  CV_TARGET_LAST,
};

// Largest paramCount() across all funcs (Probability = 4).
static const uint8_t MAX_FUNC_PARAMS = 4;

// Bytes of base-param payload persisted per channel = max over funcs
// (Euclidean = 2, Probability = 4).
static const uint8_t FUNC_PAYLOAD_MAX = 4;

// There must be a CV_PARAM_i target for every possible func parameter.
static_assert(MAX_FUNC_PARAMS <= (CV_TARGET_LAST - CV_PARAM_0),
              "CvTarget must cover all func parameters");

#endif // MODAL_FUNCS_H
