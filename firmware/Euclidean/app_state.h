/**
 * @file app_state.h
 * @author Adam Wonak (https://github.com/awonak/)
 * @brief Alt firmware version of Gravity by Sitka Instruments.
 * @version 2.0.2
 * @date 2025-07-04
 *
 * @copyright MIT - (c) 2025 - Adam Wonak - adam.wonak@gmail.com
 *
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
  byte selected_channel = 0;   // 0=tempo, 1-6=output channel
  byte selected_swing = 0;
  byte selected_save_slot = 0; // The currently active save slot.
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

#endif // APP_STATE_H