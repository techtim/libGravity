/**
 * @file func_common.h
 * @brief Shared types and the concept each channel func implements.
 *
 * A "func" is a POD state struct (so it can live in the FuncState union and be
 * memcpy'd to/from EEPROM) plus a fixed set of methods. Funcs own only their own
 * parameters; the common clock-mod, CV routing and gate timing live in Channel.
 *
 * Every func struct must provide:
 *   void reset();                                  // defaults
 *   void process(const StepContext&);              // ISR: drive the gate
 *   static uint8_t paramCount();                   // number of editable params
 *   static const __FlashStringHelper* paramLabel(uint8_t i);
 *   static const __FlashStringHelper* funcName();
 *   static void cvRange(uint8_t i, int& lo, int& hi); // bipolar CV map target
 *   int  getBase(uint8_t i) const;
 *   void setBase(uint8_t i, int value);            // clamps to the param range
 *   int  getParam(uint8_t i) const;
 *   void setParam(uint8_t i, int value);       // clamps to the param range
 *   void syncParam();                          // param = base (all)
 *   void finalize();                               // recompute derived state
 *   void save(byte* payload) const;                // write base params
 *   void load(const byte* payload);                // read base params
 */
#ifndef MODAL_FUNC_COMMON_H
#define MODAL_FUNC_COMMON_H

#include <Arduino.h>

#include "digital_output.h"

// Passed to a func's process() each clock tick. mod_pulses already reflects the
// channel's (CV-modulated) clock division.
struct StepContext {
  uint32_t tick;
  uint16_t mod_pulses;
  DigitalOutput &output;
};

#endif // MODAL_FUNC_COMMON_H
