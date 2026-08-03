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
 *               BTN MODE = INVERTED swaps these two.
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
void editMainParameter(int val, bool held);
void editChannelParameter(int val, bool held);
void editSelectedParameter(int val, bool held);
uint8_t pageParamCount();
void InitGravity(AppState &app);
void ApplyCvCal();
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
  for (uint8_t i = 0; i < Gravity::OUTPUT_COUNT; i++) {
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

void loop() {
  gravity.Process();

  // Bipolar CV readings reduced to -127..127 (Read() >> 2) so reading * amount
  // stays within a 16-bit int in applyCvMod.
  int8_t cv1 = constrain(gravity.cv1.Read() >> 2, -127, 127);
  int8_t cv2 = constrain(gravity.cv2.Read() >> 2, -127, 127);

  for (uint8_t i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    auto &ch = app.channel[i];
    if (ch.isCvActive()) {
      ch.applyCvMod(cv1, cv2);
      if (app.selected_channel == i + 1) {
        app.refresh_screen = true;
      }
    }
  }

  // Clock run from a CV gate.
  if (app.cv_run == 1 || app.cv_run == 2) {
    int8_t val = app.cv_run == 1 ? gravity.cv1.Read() >> 2 : gravity.cv2.Read() >> 2;
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

  // Keep the CV signal meter live while a calibration screen is up.
  if (app.selected_channel == 0 &&
      app.selected_param >= PARAM_MAIN_CV1_CAL_LO &&
      app.selected_param <= PARAM_MAIN_CV2_CAL_HI) {
    app.refresh_screen = true;
  }

  if (app.refresh_screen) {
    UpdateDisplay();
  }
}

//
// Clock handlers.
//

void HandleIntClockTick(uint32_t tick) {
  // Phase 1: decide every channel's output (deferred - no pins written yet).
  for (uint8_t i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    app.channel[i].processClockTick(tick, gravity.outputs[i]);
  }
  // Choke: silence any channel whose choke source's gate is high this tick.
  // Snapshot the decided gate states first so the trigger is the source's
  // natural fire - independent of channel order and of the source being choked.
  bool gate_on[Gravity::OUTPUT_COUNT];
  for (uint8_t i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    gate_on[i] = gravity.outputs[i].On();
  }
  for (uint8_t i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    uint8_t src = app.channel[i].getChoke();
    if (src != 0 && src <= Gravity::OUTPUT_COUNT && gate_on[src - 1]) {
      gravity.outputs[i].Low();
    }
  }

  // Phase 2: write all six pins together so the channels update in lockstep.
  for (uint8_t i = 0; i < Gravity::OUTPUT_COUNT; i++) {
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
    const uint16_t low_at = max(pulse_high_ticks / 2, 1);
    const uint16_t phase = tick % pulse_high_ticks;
    if (phase == 0) {
      gravity.pulse.High();
    } else if (phase == pulse_high_ticks - low_at) {
      gravity.pulse.Low();
    }
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
    gravity.clock.Tick();
    app.refresh_screen = true;
  }
}

//
// UI handlers for encoder and buttons.
//

void HandlePlayPressed() {
  if (gravity.shift_button.On() != app.invert_buttons) {
    if (app.selected_channel == 0) {
      for (uint8_t i = 0; i < Gravity::OUTPUT_COUNT; i++) {
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
      stateManager.markMetadataDirty();
      break;
    case PARAM_MAIN_ROTATE_DISP:
      app.rotate_display = app.selected_sub_param == 1;
      gravity.display.setFlipMode(app.rotate_display ? 1 : 0);
      stateManager.markMetadataDirty();
      break;
    case PARAM_MAIN_BTN_MODE:
      app.invert_buttons = app.selected_sub_param == 1;
      stateManager.markMetadataDirty();
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
    case PARAM_MAIN_BTN_MODE:
      app.selected_sub_param = app.invert_buttons ? 1 : 0; break;
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
  editSelectedParameter(val, /*held=*/true); // hold+rotate -> CV destination
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
    updateSelection(app.selected_param, val, pageParamCount());
  } else {
    editSelectedParameter(val, /*held=*/false); // click+rotate -> CV amount
  }
  app.refresh_screen = true;
}

void HandlePressedRotate(int val) {
  updateSelection(app.selected_channel, val, Gravity::OUTPUT_COUNT + 1);
  // Keep the selected param across channels; clamp to the destination page.
  const uint8_t max_param = pageParamCount();
  if (app.selected_param >= max_param) {
    app.selected_param = max_param - 1;
  }
  stateManager.markDirty();
  app.refresh_screen = true;
}

void editMainParameter(int val, bool held) {
  // CV calibration: the six items map 1:1 to cv_cal[] (live edit, held = coarse).
  if (app.selected_param >= PARAM_MAIN_CV1_CAL_LO &&
      app.selected_param <= PARAM_MAIN_CV2_CAL_HI) {
    app.cv_cal[app.selected_param - PARAM_MAIN_CV1_CAL_LO] += val * 8 * (held ? 5 : 1);
    ApplyCvCal();
    stateManager.markMetadataDirty();
    return;
  }
  switch (static_cast<ParamsMainPage>(app.selected_param)) {
  case PARAM_MAIN_TEMPO:
    if (gravity.clock.ExternalSource()) {
      break;
    }
    gravity.clock.SetTempo(gravity.clock.Tempo() + val * (held ? 5 : 1));
    app.tempo = gravity.clock.Tempo();
    break;
  // RUN / RESET / SOURCE / PULSE now live in metadata (global settings).
  case PARAM_MAIN_RUN:
    updateSelection(app.selected_sub_param, val, 3);
    app.cv_run = app.selected_sub_param;
    stateManager.markMetadataDirty();
    break;
  case PARAM_MAIN_RESET:
    updateSelection(app.selected_sub_param, val, CV_RESET_LAST);
    app.cv_reset = app.selected_sub_param;
    stateManager.markMetadataDirty();
    break;
  case PARAM_MAIN_SOURCE: {
    byte source = static_cast<byte>(app.selected_source);
    updateSelection(source, val, Clock::SOURCE_LAST);
    app.selected_source = static_cast<Clock::Source>(source);
    gravity.clock.SetSource(app.selected_source);
    stateManager.markMetadataDirty();
    break;
  }
  case PARAM_MAIN_PULSE: {
    byte pulse = static_cast<byte>(app.selected_pulse);
    updateSelection(pulse, val, Clock::PULSE_LAST);
    app.selected_pulse = static_cast<Clock::Pulse>(pulse);
    if (app.selected_pulse == Clock::PULSE_NONE) {
      gravity.pulse.Low();
    }
    stateManager.markMetadataDirty();
    break;
  }
  // Applied on encoder button press.
  case PARAM_MAIN_ENCODER_DIR:
  case PARAM_MAIN_ROTATE_DISP:
  case PARAM_MAIN_BTN_MODE:
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

// held = true for a press-and-hold rotate (selects the CV destination); false
// for a latched click-then-rotate (adjusts the CV amount).
void editChannelParameter(int val, bool held) {
  auto &ch = GetSelectedChannel();

  if (app.selected_param == CP_CLOCK_MOD) {
    ch.setClockMod(ch.getClockModIndex() + val);
  } else if (pageParamIsGate(app.selected_param)) {
    ch.editParam(app.selected_param, val);
  } else if (app.selected_param == CP_CHOKE) {
    // Choke source: 0 = off, else a 1-based channel number. A channel may not
    // choke itself, so hop over its own number (app.selected_channel).
    byte prev = ch.getChoke();
    byte src = prev;
    updateSelection(src, val, Gravity::OUTPUT_COUNT + 1); // 0..OUTPUT_COUNT
    if (src == app.selected_channel) {
      byte hopped = src;
      updateSelection(hopped, val, Gravity::OUTPUT_COUNT + 1);
      src = (hopped == app.selected_channel) ? prev : hopped; // edge: stay put
    }
    ch.setChoke(src);
  } else {
    // CV mod slot (CV1-A/B, CV2-A/B). Hold+rotate picks the destination;
    // click+rotate sets the amount (-100..100, negative inverts).
    uint8_t slot = app.selected_param - CP_CV1A;
    if (held) {
      byte t = static_cast<int>(ch.getCvDest(slot));
      updateSelection(t, val, CV_TARGET_COUNT); // CV_NONE..CV_SWING
      ch.setCvDest(slot, static_cast<CvTarget>(t));
    } else {
      ch.setCvAmount(slot, ch.getCvAmount(slot) + val);
    }
  }
}

// Number of menu rows on the page currently shown.
uint8_t pageParamCount() {
  return (app.selected_channel == 0) ? (uint8_t)PARAM_MAIN_LAST
                                     : (uint8_t)CP_PARAM_COUNT;
}

// Route an edit to whichever page is showing.
void editSelectedParameter(int val, bool held) {
  if (app.selected_channel == 0) {
    editMainParameter(val, held);
  } else {
    editChannelParameter(val, held);
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

// Push the stored per-input CV calibration into the AnalogInput objects.
// Sane calibration bounds, also clamps any garbage loaded from an old EEPROM layout.
void ApplyCvCal() {
  for (uint8_t i = 0; i < 2; i++) {
    AnalogInput &cv = i == 0 ? gravity.cv1 : gravity.cv2;
    uint8_t b = i * CAL_PER_INPUT; // [low, offset, high]
    app.cv_cal[b] = constrain(app.cv_cal[b], -1536, -100);
    app.cv_cal[b + 1] = constrain(app.cv_cal[b + 1], -1536, 1536);
    app.cv_cal[b + 2] = constrain(app.cv_cal[b + 2], 100, 1536);
    cv.SetCalibrationLow(app.cv_cal[b]);
    cv.SetCalibrationHigh(app.cv_cal[b + 2]);
    cv.AdjustOffset(app.cv_cal[b + 1] - cv.GetOffset());
  }
}

void InitGravity(AppState &app) {
  gravity.clock.SetTempo(app.tempo);
  gravity.clock.SetSource(app.selected_source);
  gravity.encoder.SetReverseDirection(app.encoder_reversed);
  gravity.display.setFlipMode(app.rotate_display ? 1 : 0);
  ApplyCvCal();
}

void ResetOutputs() {
  for (uint8_t i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    gravity.outputs[i].Low();
    gravity.outputs[i].Flush(); // outputs are deferred; write the low now
  }
}
