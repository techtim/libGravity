/**
 * @file save_state.h
 * @brief EEPROM persistence. Each channel serializes itself (Channel::save /
 *        load); the manager stores a fixed record per save slot plus a transient
 *        auto-save slot that is reloaded on boot.
 */
#ifndef SAVE_STATE_H
#define SAVE_STATE_H

#include <Arduino.h>
#include <libGravity.h>

#include "channel.h"

// Forward-declare AppState to avoid a circular include.
struct AppState;

class StateManager {
public:
  static const char SKETCH_NAME[];
  static const char SEMANTIC_VERSION[];
  static const byte MAX_SAVE_SLOTS;
  static const byte TRANSIENT_SLOT;

  StateManager();

  bool initialize(AppState &app);
  bool loadData(AppState &app, byte slot_index);
  void saveData(const AppState &app);
  void reset(AppState &app);
  void update(const AppState &app);
  void markDirty(); // EepromData changed
  void markMetadataDirty(); // Metadata changed (encoder/rotate/CV cal/etc)
  void factoryReset(AppState &app);

  struct Metadata {
    char sketch_name[16];
    char version[16];
    uint16_t layout; // struct-size signature; mismatch => stale layout => reset
    byte selected_save_slot;
    bool encoder_reversed;
    bool rotate_display;
    bool invert_buttons;
    bool future_flag;
    byte selected_source;
    byte selected_pulse;
    byte cv_run;
    byte cv_reset;
    int cv_cal[6];
  };
  struct EepromData {
    int tempo;
    byte selected_param;
    byte selected_channel;
    byte channel_data[Gravity::OUTPUT_COUNT][Channel::SAVE_BYTES];
  };

private:
  bool _isDataValid();
  void _saveMetadata(const AppState &app);
  void _loadMetadata(AppState &app);
  void _saveState(const AppState &app, byte slot_index);
  void _loadState(AppState &app, byte slot_index);

  static const unsigned long SAVE_DELAY_MS;
  static const int METADATA_START_ADDR;
  static const int EEPROM_DATA_START_ADDR;

  unsigned long _lastChangeTime;
  bool _isDirty;
  bool _isMetadataDirty;
};

#endif // SAVE_STATE_H
