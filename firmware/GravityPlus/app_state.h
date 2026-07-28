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
  bool refresh_screen = true;
  // Per-input ADC calibration (raw endpoints + zero offset). Applied to
  // gravity.cvN in InitGravity so an electrical 0V reads 0.
  int cv1_cal_low = CALIBRATED_LOW;
  int cv1_cal_high = CALIBRATED_HIGH;
  int cv1_cal_offset = 0;
  int cv2_cal_low = CALIBRATED_LOW;
  int cv2_cal_high = CALIBRATED_HIGH;
  int cv2_cal_offset = 0;
};

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
  app.editing_param = false;
  app.cv1_cal_low = CALIBRATED_LOW;
  app.cv1_cal_high = CALIBRATED_HIGH;
  app.cv1_cal_offset = 0;
  app.cv2_cal_low = CALIBRATED_LOW;
  app.cv2_cal_high = CALIBRATED_HIGH;
  app.cv2_cal_offset = 0;
  for (uint8_t i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    app.channel[i].Init();
  }
}

extern AppState app;

inline Channel &GetSelectedChannel() {
  return app.channel[app.selected_channel - 1];
}

#endif // APP_STATE_H
