/**
 * @file app_state.h
 * @brief Global app state for the Modal firmware.
 */
#ifndef APP_STATE_H
#define APP_STATE_H

#include <libGravity.h>

#include "channel.h"

// Global state for settings and app behavior.
struct AppState {
  int tempo = Clock::DEFAULT_TEMPO;
  Channel channel[Gravity::OUTPUT_COUNT];
  byte selected_param = 0;
  byte selected_sub_param = 0; // Temporary value for editing params.
  byte selected_channel = 0;   // 0=global page, 1-6=output channel
  byte selected_save_slot = 0;
  Clock::Source selected_source = Clock::SOURCE_INTERNAL;
  Clock::Pulse selected_pulse = Clock::PULSE_PPQN_24;
  byte cv_run = 0;
  byte cv_reset = 0;
  bool editing_param = false;
  bool encoder_reversed = false;
  bool rotate_display = false;
  bool refresh_screen = true;
};

extern AppState app;

static Channel &GetSelectedChannel() {
  return app.channel[app.selected_channel - 1];
}

//
// Channel-page parameter layout. The channel page shows a fixed set of params
// (func select, clock mod) followed by the current func's own params, then the
// two CV routing targets. Indices map dynamically so new funcs need no UI edits.
//
enum ChannelParamFixed : uint8_t {
  CH_PARAM_FUNC,      // select the channel's func
  CH_PARAM_CLOCK_MOD, // shared clock division
  CH_PARAM_FUNC_BASE, // func params occupy [BASE, BASE + paramCount)
};

// Upper bound on channel-page params (for fixed-size menu arrays).
static const uint8_t MAX_CHANNEL_PARAMS = CH_PARAM_FUNC_BASE + MAX_FUNC_PARAMS + 2;

// Total selectable params on the channel page for the given channel.
inline uint8_t channelParamCount(const Channel &ch) {
  return CH_PARAM_FUNC_BASE + ch.paramCount() + 2; // + CV1 + CV2 targets
}

// The two CV-target param indices sit right after the func params.
inline uint8_t channelCv1ParamIndex(const Channel &ch) {
  return CH_PARAM_FUNC_BASE + ch.paramCount();
}
inline uint8_t channelCv2ParamIndex(const Channel &ch) {
  return channelCv1ParamIndex(ch) + 1;
}

#endif // APP_STATE_H
