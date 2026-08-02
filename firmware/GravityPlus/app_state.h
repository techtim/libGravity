/**
 * @file app_state.h
 * @brief Global app state and the (fixed) channel-page parameter layout.
 */
#ifndef APP_STATE_H
#define APP_STATE_H

#include <libGravity.h>

#include "channel.h"

// cv_reset source selection.
enum CvReset : uint8_t {
  CV_RESET_NONE,
  CV_RESET_CV1,
  CV_RESET_CV2,
  CV_RESET_EXT,
  CV_RESET_LAST, // number of choices
};

// Global state for settings and app behavior.
struct AppState {
  int tempo = Clock::DEFAULT_TEMPO;
  Channel channel[Gravity::OUTPUT_COUNT];
  byte selected_param = 0;
  byte selected_sub_param = 0; // Temporary value while editing a main param.
  byte selected_channel = 0;   // 0 = global page, 1..6 = output channel
  byte selected_save_slot = 0;
  Clock::Source selected_source = Clock::SOURCE_INTERNAL;
  Clock::Pulse selected_pulse = Clock::PULSE_PPQN_24;
  byte cv_run = 0;               // 0 = none, 1 = CV1 gate, 2 = CV2 gate
  byte cv_reset = CV_RESET_NONE; // see CvReset
  bool editing_param = false;
  bool encoder_reversed = true;
  bool rotate_display = false;
  bool invert_buttons = false; // false: PLAY starts/stops, SHIFT+PLAY mutes. true: the two swap.
  bool refresh_screen = true;
  // Per-input ADC calibration, in menu order: [CV1 low, CV1 offset, CV1 high,
  // CV2 low, CV2 offset, CV2 high]. Applied to gravity.cvN in InitGravity.
  int cv_cal[6] = {CALIBRATED_LOW, 0, CALIBRATED_HIGH,
                   CALIBRATED_LOW, 0, CALIBRATED_HIGH};
};
// cv_cal[] layout: per input a low / offset / high triple.
static const uint8_t CAL_PER_INPUT = 3;

inline void ResetAppState(AppState &app) {
  app.tempo = Clock::DEFAULT_TEMPO;
  app.selected_param = 0;
  app.selected_sub_param = 0;
  app.selected_channel = 0;
  app.selected_source = Clock::SOURCE_INTERNAL;
  app.selected_pulse = Clock::PULSE_PPQN_24;
  app.cv_run = 0;
  app.cv_reset = CV_RESET_NONE;
  app.encoder_reversed = true;
  app.rotate_display = false;
  app.invert_buttons = false;
  app.editing_param = false;
  for (uint8_t i = 0; i < 6; i += CAL_PER_INPUT) {
    app.cv_cal[i] = CALIBRATED_LOW;
    app.cv_cal[i + 1] = 0;
    app.cv_cal[i + 2] = CALIBRATED_HIGH;
  }
  for (uint8_t i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    app.channel[i].Init();
  }
}

extern AppState app;

inline Channel &GetSelectedChannel() {
  return app.channel[app.selected_channel - 1];
}

#endif // APP_STATE_H
