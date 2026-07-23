/**
 * @file save_state.cpp
 * @author Adam Wonak (https://github.com/awonak/)
 * @brief Alt firmware version of Gravity by Sitka Instruments.
 * @version 2.0.2
 * @date 2025-07-04
 *
 * @copyright MIT - (c) 2025 - Adam Wonak - adam.wonak@gmail.com
 *
 */

#include "save_state.h"

#include <EEPROM.h>

#include "app_state.h"

// Define the constants for the current firmware.
const char StateManager::SKETCH_NAME[] = "ALT EUCLIDEAN";
const char StateManager::SEMANTIC_VERSION[] =
    "V2.0.2"; // NOTE: This should match the version in the
              // library.properties file.

// Number of available save slots.
const byte StateManager::MAX_SAVE_SLOTS = 10;
const byte StateManager::TRANSIENT_SLOT = 10;

// Define the minimum amount of time between EEPROM writes.
const unsigned long StateManager::SAVE_DELAY_MS = 2000;

// Calculate the starting address for EepromData, leaving space for metadata.
const int StateManager::METADATA_START_ADDR = 0;
const int StateManager::EEPROM_DATA_START_ADDR = sizeof(StateManager::Metadata);

StateManager::StateManager() : _isDirty(false), _lastChangeTime(0) {}

bool StateManager::initialize(AppState &app) {
  noInterrupts();
  bool success = false;
  if (_isDataValid()) {
    // Load global settings.
    _loadMetadata(app);
    // Load app data from the transient slot.
    _loadState(app, TRANSIENT_SLOT);
    success = true;
  }
  // EEPROM does not contain save data for this firmware & version.
  else {
    // Erase EEPROM and initialize state. Save default pattern to all save
    // slots.
    factoryReset(app);
  }
  interrupts();
  return success;
}

bool StateManager::loadData(AppState &app, byte slot_index) {
  // Check if slot_index is within max range + 1 for transient.
  if (slot_index >= MAX_SAVE_SLOTS + 1)
    return false;

  noInterrupts();

  // Load the state data from the specified EEPROM slot and update the app state
  // save slot.
  _loadState(app, slot_index);
  app.selected_save_slot = slot_index;
  // Persist this change in the global metadata on next update.
  _isDirty = true;

  interrupts();
  return true;
}

// Save app state to user specified save slot.
void StateManager::saveData(const AppState &app) {
  noInterrupts();
  // Check if slot_index is within max range + 1 for transient.
  if (app.selected_save_slot >= MAX_SAVE_SLOTS + 1) {
    interrupts();
    return;
  }

  _saveState(app, app.selected_save_slot);
  _saveMetadata(app);
  _isDirty = false;
  interrupts();
}

// Save transient state if it has changed and enough time has passed since last
// save.
void StateManager::update(const AppState &app) {
  if (_isDirty && (millis() - _lastChangeTime > SAVE_DELAY_MS)) {
    noInterrupts();
    _saveState(app, TRANSIENT_SLOT);
    _saveMetadata(app);
    _isDirty = false;
    interrupts();
  }
}

void StateManager::reset(AppState &app) {
  noInterrupts();

  AppState default_app;
  app.tempo = default_app.tempo;
  app.selected_param = default_app.selected_param;
  app.selected_channel = default_app.selected_channel;
  app.selected_source = default_app.selected_source;
  app.selected_pulse = default_app.selected_pulse;
  app.cv_run = default_app.cv_run;
  app.cv_reset = default_app.cv_reset;

  for (int i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    app.channel[i].Init();
  }

  // Load global settings from Metadata
  _loadMetadata(app);

  _isDirty = false;
  interrupts();
}

void StateManager::markDirty() {
  _isDirty = true;
  _lastChangeTime = millis();
}

// Erases all data in the EEPROM by writing 0 to every address.
void StateManager::factoryReset(AppState &app) {
  noInterrupts();
  for (unsigned int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0);
  }
  // Initialize eeprom and save default patter to all save slots.
  _saveMetadata(app);
  reset(app);
  for (int i = 0; i < MAX_SAVE_SLOTS; i++) {
    app.selected_save_slot = i;
    _saveState(app, i);
  }
  _saveState(app, TRANSIENT_SLOT);
  interrupts();
}

bool StateManager::_isDataValid() {
  Metadata metadata;
  EEPROM.get(METADATA_START_ADDR, metadata);
  bool name_match = (strcmp(metadata.sketch_name, SKETCH_NAME) == 0);
  bool version_match = (strcmp(metadata.version, SEMANTIC_VERSION) == 0);
  return name_match && version_match;
}

void StateManager::_saveState(const AppState &app, byte slot_index) {
  // Check if slot_index is within max range + 1 for transient.
  if (app.selected_save_slot >= MAX_SAVE_SLOTS + 1)
    return;

  static EepromData save_data;

  save_data.tempo = app.tempo;
  save_data.selected_param = app.selected_param;
  save_data.selected_channel = app.selected_channel;
  save_data.selected_source = static_cast<byte>(app.selected_source);
  save_data.selected_pulse = static_cast<byte>(app.selected_pulse);
  save_data.cv_run = app.cv_run;
  save_data.cv_reset = app.cv_reset;

  // TODO: break this out into a separate function. Save State should be
  // broken out into global / per-channel save methods. When saving via
  // "update" only save state for the current channel since other channels
  // will not have changed when saving user edits.
  for (int i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    const auto &ch = app.channel[i];
    auto &save_ch = save_data.channel_data[i];
    save_ch.base_clock_mod_index = ch.getClockModIndex(false);
    save_ch.base_euc_steps = ch.getSteps(false);
    save_ch.base_euc_hits = ch.getHits(false);
    save_ch.cv1_dest = static_cast<byte>(ch.getCv1Dest());
    save_ch.cv2_dest = static_cast<byte>(ch.getCv2Dest());
  }

  int address = EEPROM_DATA_START_ADDR + (slot_index * sizeof(EepromData));
  EEPROM.put(address, save_data);
}

void StateManager::_loadState(AppState &app, byte slot_index) {
  // Check if slot_index is within max range + 1 for transient.
  if (slot_index >= MAX_SAVE_SLOTS + 1)
    return;

  static EepromData load_data;
  int address = EEPROM_DATA_START_ADDR + (slot_index * sizeof(EepromData));
  EEPROM.get(address, load_data);

  // Restore app state from loaded data.
  app.tempo = load_data.tempo;
  app.selected_param = load_data.selected_param;
  app.selected_channel = load_data.selected_channel;
  app.selected_source = static_cast<Clock::Source>(load_data.selected_source);
  app.selected_pulse = static_cast<Clock::Pulse>(load_data.selected_pulse);
  app.cv_run = load_data.cv_run;
  app.cv_reset = load_data.cv_reset;

  for (int i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    auto &ch = app.channel[i];
    const auto &saved_ch_state = load_data.channel_data[i];

    ch.setClockMod(saved_ch_state.base_clock_mod_index);
    ch.setSteps(saved_ch_state.base_euc_steps);
    ch.setHits(saved_ch_state.base_euc_hits);
    ch.setCv1Dest(static_cast<CvDestination>(saved_ch_state.cv1_dest));
    ch.setCv2Dest(static_cast<CvDestination>(saved_ch_state.cv2_dest));
  }
}

void StateManager::_saveMetadata(const AppState &app) {
  Metadata current_meta;
  strcpy(current_meta.sketch_name, SKETCH_NAME);
  strcpy(current_meta.version, SEMANTIC_VERSION);

  // Global user settings
  current_meta.selected_save_slot = app.selected_save_slot;
  current_meta.encoder_reversed = app.encoder_reversed;
  current_meta.rotate_display = app.rotate_display;

  EEPROM.put(METADATA_START_ADDR, current_meta);
}

void StateManager::_loadMetadata(AppState &app) {
  Metadata metadata;
  EEPROM.get(METADATA_START_ADDR, metadata);
  app.selected_save_slot = metadata.selected_save_slot;
  app.encoder_reversed = metadata.encoder_reversed;
  app.rotate_display = metadata.rotate_display;
}