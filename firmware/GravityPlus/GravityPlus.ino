/**
 * @file GravityPlus.ino
 * @brief Alt firmware for Gravity by Sitka Instruments.
 * @version 1.0.0
 *
 * @copyright MIT
 *
 * GravityPlus gives each of the six outputs a single, powerful gate generator:
 * a Euclidean pattern (STEPS / HITS) with per-hit PROBABILITY, adjustable gate
 * length (DUTY), phase OFFSET and SWING. Setting STEPS = HITS = 1 turns a
 * channel into a pure probability gate, so this one generator replaces the
 * separate probability and euclidean firmwares.
 *
 * The internal clock runs at 96 PPQN for granular quantization of duty cycle,
 * offset and clock division. Built on the libGravity hardware library.
 *
 * ENCODER:
 *      Press: toggle between selecting a parameter and editing it.
 *      Hold & Rotate: momentary edit of the selected parameter.
 *
 * BTN1 (PLAY):  start / stop the internal clock. SHIFT + PLAY mutes.
 * BTN2 (SHIFT): hold and rotate the encoder to change the selected channel.
 *
 * EXT:  external clock in; also acts as a reset when routed via RESTART.
 * CV1 / CV2:  analog modulation for any channel parameter or the clock mod.
 */

#include <libGravity.h>

#include "app_state.h"
#include "channel.h"
#include "display.h"
#include "save_state.h"

AppState app;
StateManager stateManager;

// Forward declarations.
void updateSelection(byte &param, int change, int maxValue);
void editMainParameter(int val);
void editChannelParameter(int val);
void InitGravity(AppState &app);
void ResetOutputs();

//
// Arduino setup and loop.
//

void setup() {
  gravity.Init();

  Bootsplash();
  delay(2000);

  // Load settings from EEPROM (or factory-reset on a version change).
  stateManager.initialize(app);
  InitGravity(app);

  // The six outputs are gate-driven. Use deferred writes so the clock ISR can
  // decide all channels first and then write the pins together (minimal skew).
  for (int i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    gravity.outputs[i].SetDeferred(true);
  }

  gravity.clock.AttachIntHandler(HandleIntClockTick);
  gravity.clock.AttachExtHandler(HandleExtClockTick);

  gravity.encoder.AttachPressHandler(HandleEncoderPressed);
  gravity.encoder.AttachRotateHandler(HandleRotate);
  gravity.encoder.AttachPressRotateHandler(HandleEncoderHeldRotate);
  gravity.encoder.AttachPressRotateReleaseHandler(
      HandleEncoderReleasedAfterRotate);

  gravity.play_button.AttachPressHandler(HandlePlayPressed);
}

// Normalize a CV reading (bipolar -512..+512, 0V at 0) to a modulation value per
// the input's configured range. UNIPOLAR remaps the positive half to the full span.
int cvModValue(int read, bool unipolar) {
  return !unipolar ? read : 2 * constrain(read, 0, 512) - 512;
}

void loop() {
  gravity.Process();

  int cv1 = cvModValue(gravity.cv1.Read(), app.cv1_unipolar);
  int cv2 = cvModValue(gravity.cv2.Read(), app.cv2_unipolar);

  for (int i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    auto &ch = app.channel[i];
    if (ch.isCvActive()) {
      ch.applyCvMod(cv1, cv2);
    }
  }

  // Clock run from a CV gate.
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

  // Clock reset from a CV trigger (EXT reset is handled in the EXT interrupt).
  if ((app.cv_reset == CV_RESET_CV1 &&
       gravity.cv1.IsRisingEdge(AnalogInput::GATE_THRESHOLD)) ||
      (app.cv_reset == CV_RESET_CV2 &&
       gravity.cv2.IsRisingEdge(AnalogInput::GATE_THRESHOLD))) {
    // Drop open gates, then restart the clock (tick == 0 returns each channel's
    // pattern to step 0).
    ResetOutputs();
    gravity.clock.Reset();
  }

  stateManager.update(app);

  if (app.refresh_screen) {
    UpdateDisplay();
  }
}

//
// Clock handlers.
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
  // Phase 2: write all six pins together so the channels update in lockstep.
  for (int i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    gravity.outputs[i].Flush();
  }

  // Pulse Out gate.
  if (app.selected_pulse != Clock::PULSE_NONE) {
    int clock_index;
    switch (app.selected_pulse) {
    case Clock::PULSE_PPQN_24: clock_index = PULSE_PPQN_24_CLOCK_MOD_INDEX; break;
    case Clock::PULSE_PPQN_4: clock_index = PULSE_PPQN_4_CLOCK_MOD_INDEX; break;
    default: clock_index = PULSE_PPQN_1_CLOCK_MOD_INDEX; break;
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
    // EXT is not the clock source here; act as a reset only when routed to EXT.
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
  if (gravity.shift_button.On()) {
    if (app.selected_channel == 0) {
      for (int i = 0; i < Gravity::OUTPUT_COUNT; i++) {
        app.channel[i].toggleMute();
      }
    } else {
      GetSelectedChannel().toggleMute();
    }
    // Mute is persisted; mark dirty so the transient auto-save captures it.
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
        stateManager.loadData(app, app.selected_save_slot);
        if (gravity.clock.Tempo() != app.tempo) {
          gravity.clock.SetTempo(app.tempo);
        }
        // Apply global settings only if the clock is not running.
        if (gravity.clock.IsPaused()) {
          InitGravity(app);
        }
      }
      break;
    case PARAM_MAIN_RESET_STATE:
      if (app.selected_sub_param == 0) {
        stateManager.reset(app);
        InitGravity(app);
      }
      break;
    case PARAM_MAIN_FACTORY_RESET:
      if (app.selected_sub_param == 0) {
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

// Enter editing mode, preloading toggle-style main params from their value.
void EnterEditing() {
  if (app.selected_channel == 0) {
    switch (app.selected_param) {
    case PARAM_MAIN_ENCODER_DIR:
      app.selected_sub_param = app.encoder_reversed ? 1 : 0; break;
    case PARAM_MAIN_ROTATE_DISP:
      app.selected_sub_param = app.rotate_display ? 1 : 0; break;
    case PARAM_MAIN_CV1_RANGE:
      app.selected_sub_param = app.cv1_unipolar ? 1 : 0; break;
    case PARAM_MAIN_CV2_RANGE:
      app.selected_sub_param = app.cv2_unipolar ? 1 : 0; break;
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

// Encoder held + rotated: momentary editing of the selected parameter.
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
  if (gravity.shift_button.On()) {
    HandlePressedRotate(val);
    return;
  }
  if (!app.editing_param) {
    const int max_param =
        (app.selected_channel == 0) ? PARAM_MAIN_LAST : CHANNEL_PAGE_PARAM_COUNT;
    updateSelection(app.selected_param, val, max_param);
  } else {
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
  // Keep the selected param across channels; clamp to the destination page.
  int max_param =
      (app.selected_channel == 0) ? PARAM_MAIN_LAST : CHANNEL_PAGE_PARAM_COUNT;
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
  // Applied on encoder button press.
  case PARAM_MAIN_ENCODER_DIR:
  case PARAM_MAIN_ROTATE_DISP:
  case PARAM_MAIN_CV1_RANGE:
  case PARAM_MAIN_CV2_RANGE:
  case PARAM_MAIN_RESET_STATE:
  case PARAM_MAIN_FACTORY_RESET:
    updateSelection(app.selected_sub_param, val, 2);
    break;
  case PARAM_MAIN_SAVE_DATA:
  case PARAM_MAIN_LOAD_DATA:
    updateSelection(app.selected_sub_param, val,
                    StateManager::MAX_SAVE_SLOTS + 1);
    break;
  default:
    break;
  }
}

void editChannelParameter(int val) {
  auto &ch = GetSelectedChannel();
  const uint8_t param = app.selected_param;

  if (param == CP_CLOCK_MOD) {
    ch.setClockMod(ch.getClockModIndex() + val);
  } else if (pageParamIsGate(param)) {
    ch.editParam(pageParamToGate(param), val);
  } else {
    // CP_CV1 / CP_CV2 routing target. Valid targets are CV_NONE..CV_SWING.
    bool is_cv1 = (param == CP_CV1);
    byte t = static_cast<int>(is_cv1 ? ch.getCv1Target() : ch.getCv2Target());
    updateSelection(t, val, CV_TARGET_COUNT);
    if (is_cv1)
      ch.setCv1Target(static_cast<CvTarget>(t));
    else
      ch.setCv2Target(static_cast<CvTarget>(t));
  }
}

// Change a selection value by `change`, clamped to [0, maxValue).
void updateSelection(byte &param, int change, int maxValue) {
  // Only accelerate large ranges.
  if (maxValue < 25) {
    change = change > 0 ? 1 : -1;
  }
  param = constrain(param + change, 0, maxValue - 1);
}

//
// App helper functions.
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
    gravity.outputs[i].Flush(); // outputs are deferred; write the low now
  }
}
