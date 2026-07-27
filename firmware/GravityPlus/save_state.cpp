/**
 * @file save_state.cpp
 * @brief EEPROM persistence for the GravityPlus firmware.
 */

#include "save_state.h"

#include <EEPROM.h>

#include "app_state.h"

const char StateManager::SKETCH_NAME[] = "GRAVITY PLUS";
// Bumped for per-channel choke + per-input CV calibration (one-time reset).
const char StateManager::SEMANTIC_VERSION[] = "V1.3.0";

const byte StateManager::MAX_SAVE_SLOTS = 10;
const byte StateManager::TRANSIENT_SLOT = 10;

const unsigned long StateManager::SAVE_DELAY_MS = 2000;

const int StateManager::METADATA_START_ADDR = 0;
const int StateManager::EEPROM_DATA_START_ADDR = sizeof(StateManager::Metadata);

// The Nano's ATmega328P has 1 KB of EEPROM. Fail the build if the save data
// (11 slots + metadata) ever overflows it.
static_assert(sizeof(StateManager::EepromData) * 11 +
                      sizeof(StateManager::Metadata) <=
                  1024,
              "GravityPlus save data exceeds the ATmega328P 1KB EEPROM");

StateManager::StateManager() : _lastChangeTime(0), _isDirty(false) {}

bool StateManager::initialize(AppState &app) {
  noInterrupts();
  bool success = false;
  if (_isDataValid()) {
    _loadMetadata(app);
    _loadState(app, TRANSIENT_SLOT);
    success = true;
  } else {
    factoryReset(app);
  }
  interrupts();
  return success;
}

bool StateManager::loadData(AppState &app, byte slot_index) {
  if (slot_index >= MAX_SAVE_SLOTS + 1)
    return false;
  noInterrupts();
  _loadState(app, slot_index);
  app.selected_save_slot = slot_index;
  _isDirty = true;
  interrupts();
  return true;
}

void StateManager::saveData(const AppState &app) {
  noInterrupts();
  if (app.selected_save_slot >= MAX_SAVE_SLOTS + 1) {
    interrupts();
    return;
  }
  _saveState(app, app.selected_save_slot);
  _saveMetadata(app);
  _isDirty = false;
  interrupts();
}

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

  _loadMetadata(app);
  _isDirty = false;
  interrupts();
}

void StateManager::markDirty() {
  _isDirty = true;
  _lastChangeTime = millis();
}

void StateManager::factoryReset(AppState &app) {
  noInterrupts();
  for (unsigned int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0);
  }
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

  for (int i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    app.channel[i].save(save_data.channel_data[i]);
  }

  int address = EEPROM_DATA_START_ADDR + (slot_index * sizeof(EepromData));
  EEPROM.put(address, save_data);
}

void StateManager::_loadState(AppState &app, byte slot_index) {
  if (slot_index >= MAX_SAVE_SLOTS + 1)
    return;

  static EepromData load_data;
  int address = EEPROM_DATA_START_ADDR + (slot_index * sizeof(EepromData));
  EEPROM.get(address, load_data);

  app.tempo = load_data.tempo;
  app.selected_param = load_data.selected_param;
  app.selected_channel = load_data.selected_channel;
  app.selected_source = static_cast<Clock::Source>(load_data.selected_source);
  app.selected_pulse = static_cast<Clock::Pulse>(load_data.selected_pulse);
  app.cv_run = load_data.cv_run;
  app.cv_reset = load_data.cv_reset;

  for (int i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    app.channel[i].load(load_data.channel_data[i]);
  }
}

void StateManager::_saveMetadata(const AppState &app) {
  Metadata current_meta;
  strcpy(current_meta.sketch_name, SKETCH_NAME);
  strcpy(current_meta.version, SEMANTIC_VERSION);
  current_meta.selected_save_slot = app.selected_save_slot;
  current_meta.encoder_reversed = app.encoder_reversed;
  current_meta.rotate_display = app.rotate_display;
  current_meta.cv1_cal_low = app.cv1_cal_low;
  current_meta.cv1_cal_high = app.cv1_cal_high;
  current_meta.cv1_cal_offset = app.cv1_cal_offset;
  current_meta.cv2_cal_low = app.cv2_cal_low;
  current_meta.cv2_cal_high = app.cv2_cal_high;
  current_meta.cv2_cal_offset = app.cv2_cal_offset;
  EEPROM.put(METADATA_START_ADDR, current_meta);
}

void StateManager::_loadMetadata(AppState &app) {
  Metadata metadata;
  EEPROM.get(METADATA_START_ADDR, metadata);
  app.selected_save_slot = metadata.selected_save_slot;
  app.encoder_reversed = metadata.encoder_reversed;
  app.rotate_display = metadata.rotate_display;
  app.cv1_cal_low = metadata.cv1_cal_low;
  app.cv1_cal_high = metadata.cv1_cal_high;
  app.cv1_cal_offset = metadata.cv1_cal_offset;
  app.cv2_cal_low = metadata.cv2_cal_low;
  app.cv2_cal_high = metadata.cv2_cal_high;
  app.cv2_cal_offset = metadata.cv2_cal_offset;
}
