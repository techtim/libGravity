/**
 * @file save_state.cpp
 * @brief EEPROM persistence for the GravityPlus firmware.
 */

#include "save_state.h"

#include <avr/eeprom.h>

#include "app_state.h"

const char StateManager::SKETCH_NAME[] = "GRAVITY PLUS";
// Bumped for per-channel choke + per-input CV calibration (one-time reset).
const char StateManager::SEMANTIC_VERSION[] = "V3.1.0";

const unsigned long StateManager::SAVE_DELAY_MS = 2000;

const int StateManager::METADATA_START_ADDR = 0;
const int StateManager::EEPROM_DATA_START_ADDR = sizeof(StateManager::Metadata);

// The Nano's ATmega328P has 1 KB of EEPROM, and EEAR is only 10 bits wide - an
// address past 1023 wraps back onto the metadata at 0 instead of failing
// Check (MAX_SAVE_SLOTS + transient + metadata) ever overflows it
static_assert(sizeof(StateManager::EepromData) * (StateManager::MAX_SAVE_SLOTS + 1)
              + sizeof(StateManager::Metadata) <= 1024,
              "GravityPlus save data exceeds the ATmega328P 1KB EEPROM");

static_assert(MAX_CHOKE_SOURCE == Gravity::OUTPUT_COUNT,
              "MAX_CHOKE_SOURCE must be equal Gravity::OUTPUT_COUNT");


StateManager::StateManager()
    : _lastChangeTime(0), _isDirty(false), _isMetadataDirty(false) {}

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
  _isMetadataDirty = true; // selected_save_slot lives in metadata
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
  _isMetadataDirty = false;
  interrupts();
}

void StateManager::update(const AppState &app) {
  if (_isDirty && (millis() - _lastChangeTime > SAVE_DELAY_MS)) {
    noInterrupts();
    _saveState(app, TRANSIENT_SLOT);
    // Metadata (encoder/rotate/CV cal/slot) changes rarely, so only rewrite it
    // when actually touched - avoids an extra ~50 B EEPROM write every save.
    if (_isMetadataDirty) {
      _saveMetadata(app);
      _isMetadataDirty = false;
    }
    _isDirty = false;
    interrupts();
  }
}

void StateManager::reset(AppState &app) {
  noInterrupts();
  
  ResetAppState(app);

  _loadMetadata(app); // encoder / rotate / CV calibration
  _isDirty = false;
  interrupts();
}

void StateManager::markDirty() {
  _isDirty = true;
  _lastChangeTime = millis();
}

void StateManager::markMetadataDirty() {
  _isMetadataDirty = true;
  markDirty();
}

void StateManager::factoryReset(AppState &app) {
  noInterrupts();
  for (unsigned int i = 0; i < E2END + 1u; i++) {
    eeprom_update_byte((uint8_t *)i, 0);
  }
  // Put defaults into app FIRST, then persist them. (Do not _loadMetadata here -
  // the EEPROM was just erased, so it would read back zeros/garbage.)
  ResetAppState(app);
  app.selected_save_slot = 0;
  _saveMetadata(app);
  for (uint8_t i = 0; i < MAX_SAVE_SLOTS; i++) {
    app.selected_save_slot = i;
    _saveState(app, i);
  }
  app.selected_save_slot = 0;
  _saveState(app, TRANSIENT_SLOT);
  _isDirty = false;
  _isMetadataDirty = false;
  interrupts();
}

// Bump on ANY change to the persisted layout that keeps the struct sizes the
// same - e.g. reordering the gate params. Size changes are caught automatically
// below; order-only changes are not, so they need this.
static const uint8_t LAYOUT_REVISION = 5;

// Layout signature: struct sizes + the manual revision. A mismatch forces a
// one-time factory reset even when the version string is reused.
static uint16_t layoutSignature() {
  return (uint16_t)(sizeof(StateManager::EepromData) +
                    sizeof(StateManager::Metadata) + LAYOUT_REVISION * 7919u);
}

bool StateManager::_isDataValid() {
  Metadata metadata;
  eeprom_read_block(&metadata, (const uint8_t *)METADATA_START_ADDR, sizeof(metadata));
  bool name_match = (strcmp(metadata.sketch_name, SKETCH_NAME) == 0);
  bool version_match = (strcmp(metadata.version, SEMANTIC_VERSION) == 0);
  bool layout_match = (metadata.layout == layoutSignature());
  return name_match && version_match && layout_match;
}

void StateManager::_saveState(const AppState &app, byte slot_index) {
  if (slot_index >= MAX_SAVE_SLOTS + 1)
    return;

  // Block ops, not a byte loop: one call per field instead of one per byte.
  uint8_t *addr = (uint8_t *)(EEPROM_DATA_START_ADDR + (int)(slot_index * sizeof(EepromData)));
  eeprom_update_block(&app.tempo, addr, sizeof(app.tempo));
  addr += sizeof(app.tempo);

  byte record[Channel::SAVE_BYTES];
  for (uint8_t i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    app.channel[i].save(record);
    eeprom_update_block(record, addr, Channel::SAVE_BYTES);
    addr += Channel::SAVE_BYTES;
  }
}

void StateManager::_loadState(AppState &app, byte slot_index) {
  if (slot_index >= MAX_SAVE_SLOTS + 1)
    return;

  const uint8_t *addr = (const uint8_t *)(EEPROM_DATA_START_ADDR +
                                          (int)(slot_index * sizeof(EepromData)));
  eeprom_read_block(&app.tempo, addr, sizeof(app.tempo));
  addr += sizeof(app.tempo);
  // selected_param / selected_channel are no longer persisted: they are pure
  // UI position, and UpdateDisplay() bounds them every frame anyway.

  byte record[Channel::SAVE_BYTES];
  for (uint8_t i = 0; i < Gravity::OUTPUT_COUNT; i++) {
    eeprom_read_block(record, addr, Channel::SAVE_BYTES);
    addr += Channel::SAVE_BYTES;
    app.channel[i].load(record);
  }
}

void StateManager::_saveMetadata(const AppState &app) {
  Metadata current_meta;
  strcpy(current_meta.sketch_name, SKETCH_NAME);
  strcpy(current_meta.version, SEMANTIC_VERSION);
  current_meta.layout = layoutSignature();
  current_meta.selected_save_slot = app.selected_save_slot;
  current_meta.encoder_reversed = app.encoder_reversed;
  current_meta.rotate_display = app.rotate_display;
  current_meta.invert_buttons = app.invert_buttons;
  current_meta.selected_source = static_cast<byte>(app.selected_source);
  current_meta.selected_pulse = static_cast<byte>(app.selected_pulse);
  current_meta.cv_run = app.cv_run;
  current_meta.cv_reset = app.cv_reset;
  current_meta.midi_out = app.midi_out;
  for (uint8_t i = 0; i < 6; i++)
    current_meta.cv_cal[i] = app.cv_cal[i];
  eeprom_update_block(&current_meta, (uint8_t *)METADATA_START_ADDR, sizeof(current_meta));
}

void StateManager::_loadMetadata(AppState &app) {
  Metadata metadata;
  eeprom_read_block(&metadata, (const uint8_t *)METADATA_START_ADDR, sizeof(metadata));
  app.selected_save_slot = metadata.selected_save_slot;
  if (app.selected_save_slot >= MAX_SAVE_SLOTS)
    app.selected_save_slot = 0;
  app.encoder_reversed = metadata.encoder_reversed != false;
  app.rotate_display = metadata.rotate_display != false;
  app.invert_buttons = metadata.invert_buttons != false;
  app.selected_source = static_cast<Clock::Source>(metadata.selected_source);
  app.selected_pulse = static_cast<Clock::Pulse>(metadata.selected_pulse);
  app.cv_run = metadata.cv_run;
  app.cv_reset = metadata.cv_reset;
  app.midi_out = metadata.midi_out < MIDI_OUT_LAST ? metadata.midi_out : MIDI_OUT_CLK;
  for (uint8_t i = 0; i < 6; i++)
    app.cv_cal[i] = metadata.cv_cal[i];
}
