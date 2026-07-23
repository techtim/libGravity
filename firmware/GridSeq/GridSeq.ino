/**
 * @file GridSeq.ino
 * @author Adam Wonak (https://github.com/awonak/)
 * @brief Grid based step sequencer firmware for Gravity by Sitka Instruments.
 * @version v1.0.0 - August 2025 awonak
 * @date 2025-08-12
 *
 * @copyright MIT - (c) 2025 - Adam Wonak - adam.wonak@gmail.com
 *
 * Grid based step sequencer with lots of dynamic features.
 * 
 * Pattern:
 *      - length
 *      - clock division
 *      - probability
 *      - fill density
 *      - direction (fwd, rev, pend, rand)
 *      - mode:
 *          - step equencer
 *          - euclidean rhythm
 *          - pattern (grids like presets)
 * 
 * Step:
 *      - gate / trigger
 *      - duty / duration
 *      - probability
 *      - ratchet / retrig
 * 
 * Global:
 *      - internal / external / midi
 *      - run / reset
 *      - mute
 *      - save / load banks
 *      - 6 channel / 3 channel accent
 *
 * ENCODER:
 *      Press: change between selecting a parameter and editing the parameter.
 *      Hold & Rotate: change current selected output channel.
 *
 * BTN1:
 *      Play/pause - start or stop the internal clock.
 *
 * BTN2:
 *      Shift - hold and rotate encoder to change current selected output channel.
 *
 * EXT:
 *      External clock input. When Gravity is set to INTERNAL or MIDI clock
 *      source, this input is used to reset clocks.
 *
 * CV1:
 *      External analog input used to provide modulation to any channel parameter.
 *
 * CV2:
 *      External analog input used to provide modulation to any channel parameter.
 *
 */

#include <libGravity.h>

#include "app_state.h"
#include "channel.h"
#include "display.h"

AppState app;

//
// Arduino setup and loop.
//

void setup() {
    // Start Gravity.
    gravity.Init();

    // Clock handlers.
    gravity.clock.AttachIntHandler(HandleIntClockTick);
    gravity.clock.AttachExtHandler(HandleExtClockTick);

    // Encoder rotate and press handlers.
    gravity.encoder.AttachPressHandler(HandleEncoderPressed);
    gravity.encoder.AttachRotateHandler(HandleRotate);
    gravity.encoder.AttachPressRotateHandler(HandlePressedRotate);

    // Button press handlers.
    gravity.play_button.AttachPressHandler(HandlePlayPressed);
}

void loop() {
    // Process change in state of inputs and outputs.
    gravity.Process();

    // Check if cv run or reset is active and read cv.
    CheckRunReset(gravity.cv1, gravity.cv2);

    if (app.refresh_screen) {
        UpdateDisplay();
    }
}

//
// Firmware handlers for clocks.
//

void HandleIntClockTick(uint32_t tick) {
    bool refresh = false;
    for (int i = 0; i < Gravity::OUTPUT_COUNT; i++) {
        app.channel[i].processClockTick(tick, gravity.outputs[i]);
    }
}

void HandleExtClockTick() {
    switch (app.selected_source) {
        case Clock::SOURCE_INTERNAL:
        case Clock::SOURCE_EXTERNAL_MIDI:
            // Use EXT as Reset when not used for clock source.
            ResetOutputs();
            gravity.clock.Reset();
            break;
        default:
            // Register EXT cv clock tick.
            gravity.clock.Tick();
    }
    app.refresh_screen = true;
}

void CheckRunReset(AnalogInput& cv1, AnalogInput& cv2) {
    // Clock Run
    if (app.cv_run == 1 || app.cv_run == 2) {
        const int val = (app.cv_run == 1) ? cv1.Read() : cv2.Read();
        if (val > AnalogInput::GATE_THRESHOLD && gravity.clock.IsPaused()) {
            gravity.clock.Start();
            app.refresh_screen = true;
        } else if (val < AnalogInput::GATE_THRESHOLD && !gravity.clock.IsPaused()) {
            gravity.clock.Stop();
            ResetOutputs();
            app.refresh_screen = true;
        }
    }

    // Clock Reset
    if ((app.cv_reset == 1 && cv1.IsRisingEdge(AnalogInput::GATE_THRESHOLD)) ||
        (app.cv_reset == 2 && cv2.IsRisingEdge(AnalogInput::GATE_THRESHOLD))) {
        gravity.clock.Reset();
    }
}

//
// UI handlers for encoder and buttons.
//

void HandlePlayPressed() {
}

void HandleEncoderPressed() {
}

void HandleRotate(int val) {
}

void HandlePressedRotate(int val) {
}

// TODO: move to libGravity
void ResetOutputs() {
    for (int i = 0; i < Gravity::OUTPUT_COUNT; i++) {
        gravity.outputs[i].Low();
    }
}
