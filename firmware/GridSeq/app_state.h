/**
 * @file app_state.h
 * @author Adam Wonak (https://github.com/awonak/)
 * @brief Alt firmware version of Gravity by Sitka Instruments.
 * @version 2.0.1
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
    Clock::Source selected_source = Clock::SOURCE_INTERNAL;
    Channel channel[Gravity::OUTPUT_COUNT];
    byte selected_param = 0;
    byte selected_channel = 0;    // 0=tempo, 1-6=output channel
    byte cv_run = 0;
    byte cv_reset = 0;
    bool editing_param = false;
    bool refresh_screen = true;
};

extern AppState app;

static Channel& GetSelectedChannel() {
    return app.channel[app.selected_channel - 1];
}

#endif  // APP_STATE_H