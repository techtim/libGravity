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
  bool encoder_reversed = false;
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

extern AppState app;

inline Channel &GetSelectedChannel() {
  return app.channel[app.selected_channel - 1];
}

// The channel page shows a fixed list of parameters: clock mod, the six gate
// parameters (STEPS..SWING, in GateParam order), the choke source, then the two
// CV routing targets. The gate params occupy [CP_STEPS, CP_STEPS + GATE_PARAM_COUNT).
enum ChannelPageParam : uint8_t {
  CP_CLOCK_MOD,
  CP_STEPS,
  CP_HITS,
  CP_PROB,
  CP_DUTY,
  CP_OFFSET,
  CP_SWING,
  CP_CHOKE,
  CP_CV1,
  CP_CV2,
  CHANNEL_PAGE_PARAM_COUNT,
};
static_assert(CP_STEPS == 1 && CP_CHOKE == CP_STEPS + GATE_PARAM_COUNT,
              "channel-page gate params must be contiguous after CP_CLOCK_MOD");

// Map a channel-page param index to its GateParam (only valid for CP_STEPS..CP_SWING).
inline uint8_t pageParamToGate(uint8_t page_param) {
  return page_param - CP_STEPS;
}
inline bool pageParamIsGate(uint8_t page_param) {
  return page_param >= CP_STEPS && page_param <= CP_SWING;
}

#endif // APP_STATE_H
