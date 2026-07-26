/**
 * @file Modal.ino
 * @author Adam Wonak (https://github.com/awonak/)
 * @brief Unified "Modal" alt firmware: per-channel selectable funcs.
 * @version 2.0.2 - awonak
 * @date 2026-02-21
 *
 * @copyright MIT - (c) 2026 - Adam Wonak - adam.wonak@gmail.com
 *
 * This firmware unifies the Gravity (probability) and Euclidean firmwares: each
 * of the six output channels independently selects a func (currently PROB or
 * EUCLID) sharing the common clock, CV routing and UI. New funcs are added under
 * funcs/ and registered in funcs.h (see FUNC_LIST) with no changes to the UI or
 * save logic. Built on the libGravity hardware abstraction library.
 *
 * The internal clock runs at 96 PPQN for granular quantization of duty cycle,
 * offset and clock division.
 *
 * On the channel page, the first parameter selects the channel's FUNC; the
 * remaining parameters (clock mod, the func's own params, CV1/CV2 routing) adapt
 * to the selected func.
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

  // The six channel outputs are gate-driven by the funcs. Use deferred writes so
  // the clock ISR can decide all channels first and then write the pins together
  // (minimal skew between channels).
  for (int i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    gravity.outputs[i].SetDeferred(true);
  }

  // Clock handlers.
  gravity.clock.AttachIntHandler(HandleIntClockTick);
  gravity.clock.AttachExtHandler(HandleExtClockTick);

  // Encoder rotate and press handlers.
  gravity.encoder.AttachPressHandler(HandleEncoderPressed);
  gravity.encoder.AttachRotateHandler(HandleRotate);
  // Press + rotate edits the selected parameter's value (momentary editing);
  // release leaves editing mode. Channel selection is on shift + rotate.
  gravity.encoder.AttachPressRotateHandler(HandleEncoderHeldRotate);
  gravity.encoder.AttachPressRotateReleaseHandler(HandleEncoderReleasedAfterRotate);

  // Button press handlers.
  gravity.play_button.AttachPressHandler(HandlePlayPressed);
}

// Normalize a CV reading (AnalogInput::Read() is bipolar, -512..+512, 0V at 0)
// to a modulation value per the input's configured range:
//   BIPOLAR  (-5..+5V): pass through; 0V = no modulation, ends = full +/-.
//   UNIPOLAR (0..+5V):  the positive half (0..512) is remapped to the full -512..+512 span
int cvModValue(int read, bool unipolar) {
  return !unipolar ? read : 2 * constrain(read, 0, 512) - 512;
}

void loop() {
  // Process change in state of inputs and outputs.
  gravity.Process();

  // Read CVs and call the update function for each channel. The modulation
  // value is normalized to the input's configured range (see cvModValue).
  int cv1 = cvModValue(gravity.cv1.Read(), app.cv1_unipolar);
  int cv2 = cvModValue(gravity.cv2.Read(), app.cv2_unipolar);

  for (int i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    auto &ch = app.channel[i];
    // Only apply CV to the channel when the current channel has cv mod configured.
    if (ch.isCvActive()) {
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

  // Clock Reset from CV (EXT reset is handled by the EXT pin interrupt).
  if ((app.cv_reset == CV_RESET_CV1 &&
       gravity.cv1.IsRisingEdge(AnalogInput::GATE_THRESHOLD)) ||
      (app.cv_reset == CV_RESET_CV2 &&
       gravity.cv2.IsRisingEdge(AnalogInput::GATE_THRESHOLD))) {
    // Match the EXT reset path: drop any open gates, then restart the clock (the
    // tick == 0 restart returns each channel's pattern to its start).
    ResetOutputs();
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
  // Phase 1: decide every channel's output (deferred - no pins written yet).
  for (int i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    app.channel[i].processClockTick(tick, gravity.outputs[i]);

    if (app.channel[i].isCvActive()) {
      refresh = true;
    }
  }
  // Phase 2: write all six pins in a tight loop so the channels update together
  // (the per-channel decision cost no longer sits between the pin writes).
  for (int i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    gravity.outputs[i].Flush();
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
    // EXT is not the clock source here. Only act as a reset when user has routed cv_reset to EXT
    if (app.cv_reset == CV_RESET_EXT) {
      ResetOutputs();
      gravity.clock.Reset();
      app.refresh_screen = true;
    }
    break;
  default:
    // EXT is the clock source: register the external tick.
    gravity.clock.Tick();
    app.refresh_screen = true;
  }
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
    // Mute is persisted per channel; without this the transient auto-save never
    // fires for a mute change and it's lost on power cycle.
    stateManager.markDirty();
  } else {
    gravity.clock.IsPaused() ? gravity.clock.Start() : gravity.clock.Stop();
    ResetOutputs();
  }
  app.refresh_screen = true;
}

// Apply any pending main-page selection, then leave editing mode.
void ExitEditing() {
  if (app.selected_channel == 0) { // main page
    switch (app.selected_param) {
    case PARAM_MAIN_ENCODER_DIR:
      app.encoder_reversed = app.selected_sub_param == 1;
      gravity.encoder.SetReverseDirection(app.encoder_reversed);
      break;
    case PARAM_MAIN_ROTATE_DISP:
      app.rotate_display = app.selected_sub_param == 1;
      gravity.display.setFlipMode(app.rotate_display ? 1 : 0);
      break;
    case PARAM_MAIN_CV1_RANGE:
      app.cv1_unipolar = app.selected_sub_param == 1;
      break;
    case PARAM_MAIN_CV2_RANGE:
      app.cv2_unipolar = app.selected_sub_param == 1;
      break;
    case PARAM_MAIN_SAVE_DATA:
      if (app.selected_sub_param < StateManager::MAX_SAVE_SLOTS) {
        app.selected_save_slot = app.selected_sub_param;
        stateManager.saveData(app);
      }
      break;
    case PARAM_MAIN_LOAD_DATA:
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
      break;
    case PARAM_MAIN_RESET_STATE:
      if (app.selected_sub_param == 0) { // Reset
        stateManager.reset(app);
        InitGravity(app);
      }
      break;
    case PARAM_MAIN_FACTORY_RESET:
      if (app.selected_sub_param == 0) { // Erase
        // Show bootsplash during slow erase operation.
        Bootsplash();
        stateManager.factoryReset(app);
        InitGravity(app);
      }
      break;
    default:
      break;
    }
  }
  stateManager.markDirty();
  app.selected_sub_param = 0;
  app.editing_param = false;
}

// Enter editing mode, preloading toggle-style params from their current value.
void EnterEditing() {
  if (app.selected_channel == 0) {
    switch (app.selected_param) {
    case PARAM_MAIN_ENCODER_DIR:
      app.selected_sub_param = app.encoder_reversed ? 1 : 0;
      break;
    case PARAM_MAIN_ROTATE_DISP:
      app.selected_sub_param = app.rotate_display ? 1 : 0;
      break;
    case PARAM_MAIN_CV1_RANGE:
      app.selected_sub_param = app.cv1_unipolar ? 1 : 0;
      break;
    case PARAM_MAIN_CV2_RANGE:
      app.selected_sub_param = app.cv2_unipolar ? 1 : 0;
      break;
    default:
      break;
    }
  }
  app.editing_param = true;
}

// Encoder click (no rotation): toggle editing mode (latched).
void HandleEncoderPressed() {
  app.editing_param ? ExitEditing() : EnterEditing();
  app.refresh_screen = true;
}

// Encoder held + rotated: momentary editing of the selected parameter's value.
void HandleEncoderHeldRotate(int val) {
  if (!app.editing_param) {
    EnterEditing();
  }
  if (app.selected_channel == 0) {
    editMainParameter(val);
  } else {
    editChannelParameter(val);
  }
  app.refresh_screen = true;
}

// Encoder released after a held-rotate: leave editing mode.
void HandleEncoderReleasedAfterRotate() {
  ExitEditing();
  app.refresh_screen = true;
}

void HandleRotate(int val) {
  // Shift & Rotate check
  if (gravity.shift_button.On()) {
    HandlePressedRotate(val);
    return;
  }

  if (!app.editing_param) {
    // Navigation Func
    const int max_param =
        (app.selected_channel == 0) ? PARAM_MAIN_LAST
                                    : channelParamCount(GetSelectedChannel());
    updateSelection(app.selected_param, val, max_param);
  } else {
    // Editing Func
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
  // Keep the selected param when switching channels (fast tweaking of the same
  // param across channels); clamp it to the new channel's parameter count.
  int max_param = (app.selected_channel == 0)
                      ? PARAM_MAIN_LAST
                      : channelParamCount(GetSelectedChannel());
  if (app.selected_param >= max_param) {
    app.selected_param = max_param - 1;
  }
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
    updateSelection(app.selected_sub_param, val, CV_RESET_LAST);
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
  case PARAM_MAIN_CV1_RANGE:
    updateSelection(app.selected_sub_param, val, 2);
    break;
  case PARAM_MAIN_CV2_RANGE:
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
  const uint8_t param = app.selected_param;
  const uint8_t cv1_idx = channelCv1ParamIndex(ch);
  const uint8_t cv2_idx = channelCv2ParamIndex(ch);

  if (param == CH_PARAM_FUNC) {
    byte m = static_cast<int>(ch.getFunc());
    updateSelection(m, val, FUNC_LAST);
    ch.setFunc(static_cast<ChannelFunc>(m)); // clears CV routing on change
  } else if (param == CH_PARAM_CLOCK_MOD) {
    ch.setClockMod(ch.getClockModIndex() + val);
  } else if (param < cv1_idx) {
    // Func parameter.
    ch.editParam(param - CH_PARAM_FUNC_BASE, val);
  } else {
    // CV1 / CV2 routing target. Valid targets are contiguous:
    // NONE, CLOCK_MOD, then one PARAM_i per func parameter.
    bool is_cv1 = (param == cv1_idx);
    byte t = static_cast<int>(is_cv1 ? ch.getCv1Target() : ch.getCv2Target());
    updateSelection(t, val, CV_PARAM_0 + ch.paramCount());
    if (is_cv1)
      ch.setCv1Target(static_cast<CvTarget>(t));
    else
      ch.setCv2Target(static_cast<CvTarget>(t));
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
    gravity.outputs[i].Flush(); // outputs are deferred; write the low immediately
  }
}
