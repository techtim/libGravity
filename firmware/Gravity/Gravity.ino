/**
 * @file Gravity.ino
 * @author Adam Wonak (https://github.com/awonak/)
 * @brief Alt firmware version of Gravity by Sitka Instruments.
 * @version 2.0.2 - awonak
 * @date 2026-02-21
 *
 * @copyright MIT - (c) 2026 - Adam Wonak - adam.wonak@gmail.com
 *
 * This version of Gravity firmware is a full rewrite that leverages the
 * libGravity hardware abstraction library. The goal of this project was to
 * create an open source friendly version of the firmware that makes it easy
 * for users/developers to modify and create their own original alt firmware
 * implementations.
 *
 * The libGravity library represents wrappers around the
 * hardware peripherials to make it easy to interact with and add behavior
 * to them. The library tries not to make any assumptions about what the
 * firmware can or should do.
 *
 * The Gravity firmware is a slightly different implementation of the original
 * firmware. There are a few notable changes; the internal clock operates at
 * 96 PPQN instead of the original 24 PPQN, which allows for more granular
 * quantization of features like duty cycle (pulse width) or offset.
 * Additionally, this firmware replaces the sequencer with a Euclidean Rhythm
 * generator.
 *
 * ENCODER:
 *      Press: change between selecting a parameter and editing the parameter.
 *      Hold & Rotate: change current selected output channel.
 *
 * BTN1:
 *      Play/pause - start or stop the internal clock.
 *
 * BTN2:
 *      Shift - hold and rotate encoder to change current selected output
 * channel.
 *
 * EXT:
 *      External clock input. When Gravity is set to INTERNAL or MIDI clock
 *      source, this input is used to reset clocks.
 *
 * CV1:
 *      External analog input used to provide modulation to any channel
 * parameter.
 *
 * CV2:
 *      External analog input used to provide modulation to any channel
 * parameter.
 *
 */

#include <libGravity.h>

#include "app_state.h"
#include "channel.h"
#include "display.h"
#include "save_state.h"

AppState app;
StateManager stateManager;

//
// Arduino setup and loop.
//

void setup() {
  // Start Gravity.
  gravity.Init();

  // Show bootsplash when initializing firmware.
  Bootsplash();
  delay(2000);

  // Initialize the state manager. This will load settings from EEPROM
  stateManager.initialize(app);
  InitGravity(app);

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

  // Read CVs and call the update function for each channel.
  int cv1 = gravity.cv1.Read();
  int cv2 = gravity.cv2.Read();

  for (int i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    auto &ch = app.channel[i];
    // Only apply CV to the channel when the current channel has cv
    // mod configured.
    if (ch.isCvModActive()) {
      ch.applyCvMod(cv1, cv2);
    }
  }

  // Clock Run
  if (app.cv_run == 1 || app.cv_run == 2) {
    auto &cv = app.cv_run == 1 ? gravity.cv1 : gravity.cv2;
    int val = cv.Read();
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
  if ((app.cv_reset == 1 &&
       gravity.cv1.IsRisingEdge(AnalogInput::GATE_THRESHOLD)) ||
      (app.cv_reset == 2 &&
       gravity.cv2.IsRisingEdge(AnalogInput::GATE_THRESHOLD))) {
    gravity.clock.Reset();
  }

  // Check for dirty state eligible to be saved.
  stateManager.update(app);

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

    if (app.channel[i].isCvModActive()) {
      refresh = true;
    }
  }

  // Pulse Out gate
  if (app.selected_pulse != Clock::PULSE_NONE) {
    int clock_index;
    switch (app.selected_pulse) {
    case Clock::PULSE_PPQN_24:
      clock_index = PULSE_PPQN_24_CLOCK_MOD_INDEX;
      break;
    case Clock::PULSE_PPQN_4:
      clock_index = PULSE_PPQN_4_CLOCK_MOD_INDEX;
      break;
    case Clock::PULSE_PPQN_1:
      clock_index = PULSE_PPQN_1_CLOCK_MOD_INDEX;
      break;
    }

    const uint16_t pulse_high_ticks =
        pgm_read_word_near(&CLOCK_MOD_PULSES[clock_index]);
    const uint32_t pulse_low_ticks = tick + max((pulse_high_ticks / 2), 1L);

    if (tick % pulse_high_ticks == 0) {
      gravity.pulse.High();
    } else if (pulse_low_ticks % pulse_high_ticks == 0) {
      gravity.pulse.Low();
    }
  }

  if (!app.editing_param) {
    app.refresh_screen |= refresh;
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

//
// UI handlers for encoder and buttons.
//

void HandlePlayPressed() {
  // Check if SHIFT is pressed to mute all/current channel.
  if (gravity.shift_button.On()) {
    if (app.selected_channel == 0) {
      // Mute all channels
      for (int i = 0; i < Gravity::OUTPUT_COUNT; i++) {
        app.channel[i].toggleMute();
      }
    } else {
      // Mute selected channel
      auto &ch = GetSelectedChannel();
      ch.toggleMute();
    }
    return;
  }

  gravity.clock.IsPaused() ? gravity.clock.Start() : gravity.clock.Stop();
  ResetOutputs();
  app.refresh_screen = true;
}

void HandleEncoderPressed() {
  // Check if leaving editing mode should apply a selection.
  if (app.editing_param) {
    if (app.selected_channel == 0) { // main page
      // TODO: rewrite as switch
      if (app.selected_param == PARAM_MAIN_ENCODER_DIR) {
        app.encoder_reversed = app.selected_sub_param == 1;
        gravity.encoder.SetReverseDirection(app.encoder_reversed);
      }
      if (app.selected_param == PARAM_MAIN_ROTATE_DISP) {
        app.rotate_display = app.selected_sub_param == 1;
        gravity.display.setFlipMode(app.rotate_display ? 1 : 0);
      }
      if (app.selected_param == PARAM_MAIN_SAVE_DATA) {
        if (app.selected_sub_param < StateManager::MAX_SAVE_SLOTS) {
          app.selected_save_slot = app.selected_sub_param;
          stateManager.saveData(app);
        }
      }
      if (app.selected_param == PARAM_MAIN_LOAD_DATA) {
        if (app.selected_sub_param < StateManager::MAX_SAVE_SLOTS) {
          app.selected_save_slot = app.selected_sub_param;
          // Load pattern data into app state.
          stateManager.loadData(app, app.selected_save_slot);
          // Load global performance settings if they have changed.
          if (gravity.clock.Tempo() != app.tempo) {
            gravity.clock.SetTempo(app.tempo);
          }
          // Load global settings only if clock is not active.
          if (gravity.clock.IsPaused()) {
            InitGravity(app);
          }
        }
      }
      if (app.selected_param == PARAM_MAIN_RESET_STATE) {
        if (app.selected_sub_param == 0) { // Reset
          stateManager.reset(app);
          InitGravity(app);
        }
      }
      if (app.selected_param == PARAM_MAIN_FACTORY_RESET) {
        if (app.selected_sub_param == 0) { // Erase
          // Show bootsplash during slow erase operation.
          Bootsplash();
          stateManager.factoryReset(app);
          InitGravity(app);
        }
      }
    }
    // Only mark dirty and reset selected_sub_param when leaving editing mode.
    stateManager.markDirty();
    app.selected_sub_param = 0;
  }

  app.editing_param = !app.editing_param;
  app.refresh_screen = true;
}

void HandleRotate(int val) {
  // Shift & Rotate check
  if (gravity.shift_button.On()) {
    HandlePressedRotate(val);
    return;
  }

  if (!app.editing_param) {
    // Navigation Mode
    const int max_param =
        (app.selected_channel == 0) ? PARAM_MAIN_LAST : PARAM_CH_LAST;
    updateSelection(app.selected_param, val, max_param);
  } else {
    // Editing Mode
    if (app.selected_channel == 0) {
      editMainParameter(val);
    } else {
      editChannelParameter(val);
    }
  }
  app.refresh_screen = true;
}

void HandlePressedRotate(int val) {
  updateSelection(app.selected_channel, val, Gravity::OUTPUT_COUNT + 1);
  app.selected_param = 0;
  stateManager.markDirty();
  app.refresh_screen = true;
}

void editMainParameter(int val) {
  switch (static_cast<ParamsMainPage>(app.selected_param)) {
  case PARAM_MAIN_TEMPO:
    if (gravity.clock.ExternalSource()) {
      break;
    }
    gravity.clock.SetTempo(gravity.clock.Tempo() + val);
    app.tempo = gravity.clock.Tempo();
    break;
  case PARAM_MAIN_RUN:
    updateSelection(app.selected_sub_param, val, 3);
    app.cv_run = app.selected_sub_param;
    break;
  case PARAM_MAIN_RESET:
    updateSelection(app.selected_sub_param, val, 3);
    app.cv_reset = app.selected_sub_param;
    break;
  case PARAM_MAIN_SOURCE: {
    byte source = static_cast<int>(app.selected_source);
    updateSelection(source, val, Clock::SOURCE_LAST);
    app.selected_source = static_cast<Clock::Source>(source);
    gravity.clock.SetSource(app.selected_source);
    break;
  }
  case PARAM_MAIN_PULSE: {
    byte pulse = static_cast<int>(app.selected_pulse);
    updateSelection(pulse, val, Clock::PULSE_LAST);
    app.selected_pulse = static_cast<Clock::Pulse>(pulse);
    if (app.selected_pulse == Clock::PULSE_NONE) {
      gravity.pulse.Low();
    }
    break;
  }
  // These changes are applied upon encoder button press.
  case PARAM_MAIN_ENCODER_DIR:
    updateSelection(app.selected_sub_param, val, 2);
    break;
  case PARAM_MAIN_ROTATE_DISP:
    updateSelection(app.selected_sub_param, val, 2);
    break;
  case PARAM_MAIN_SAVE_DATA:
  case PARAM_MAIN_LOAD_DATA:
    updateSelection(app.selected_sub_param, val,
                    StateManager::MAX_SAVE_SLOTS + 1);
    break;
  case PARAM_MAIN_RESET_STATE:
    updateSelection(app.selected_sub_param, val, 2);
    break;
  case PARAM_MAIN_FACTORY_RESET:
    updateSelection(app.selected_sub_param, val, 2);
    break;
  }
}

void editChannelParameter(int val) {
  auto &ch = GetSelectedChannel();
  switch (app.selected_param) {
  case PARAM_CH_MOD:
    ch.setClockMod(ch.getClockModIndex() + val);
    break;
  case PARAM_CH_PROB:
    ch.setProbability(ch.getProbability() + val);
    break;
  case PARAM_CH_DUTY:
    ch.setDutyCycle(ch.getDutyCycle() + val);
    break;
  case PARAM_CH_OFFSET:
    ch.setOffset(ch.getOffset() + val);
    break;
  case PARAM_CH_SWING:
    ch.setSwing(ch.getSwing() + val);
    break;
  case PARAM_CH_CV1_DEST: {
    byte dest = static_cast<int>(ch.getCv1Dest());
    updateSelection(dest, val, CV_DEST_LAST);
    ch.setCv1Dest(static_cast<CvDestination>(dest));
    break;
  }
  case PARAM_CH_CV2_DEST: {
    byte dest = static_cast<int>(ch.getCv2Dest());
    updateSelection(dest, val, CV_DEST_LAST);
    ch.setCv2Dest(static_cast<CvDestination>(dest));
    break;
  }
  }
}

// Changes the param by the value provided.
void updateSelection(byte &param, int change, int maxValue) {
  // Do not apply acceleration if max value is less than 25.
  if (maxValue < 25) {
    change = change > 0 ? 1 : -1;
  }
  param = constrain(param + change, 0, maxValue - 1);
}

//
// App Helper functions.
//

void InitGravity(AppState &app) {
  gravity.clock.SetTempo(app.tempo);
  gravity.clock.SetSource(app.selected_source);
  gravity.encoder.SetReverseDirection(app.encoder_reversed);
  gravity.display.setFlipMode(app.rotate_display ? 1 : 0);
}

void ResetOutputs() {
  for (int i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    gravity.outputs[i].Low();
  }
}
