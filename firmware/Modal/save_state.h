/**
 * @file save_state.h
 * @brief EEPROM persistence for the Modal firmware. Per-channel records store
 *        the func tag + common fields + a capped func-payload union of base
 *        params (see FUNC_PAYLOAD_MAX in funcs.h).
 */
#ifndef SAVE_STATE_H
#define SAVE_STATE_H

#include <Arduino.h>
#include <libGravity.h>

#include "funcs.h"

// Forward-declare AppState to avoid circular dependencies.
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
  void markDirty();
  void factoryReset(AppState &app);

  struct Metadata {
    char sketch_name[16];
    char version[16];
    byte selected_save_slot;
    bool encoder_reversed;
    bool rotate_display;
  };
  // Per-channel persisted record. flags bit0 = mute.
  struct ChannelState {
    byte func;
    byte base_clock_mod_index;
    byte cv1_target;
    byte cv2_target;
    byte flags;
    byte payload[FUNC_PAYLOAD_MAX];
  };
  struct EepromData {
    int tempo;
    byte selected_param;
    byte selected_channel;
    byte selected_source;
    byte selected_pulse;
    byte cv_run;
    byte cv_reset;
    ChannelState channel_data[Gravity::OUTPUT_COUNT];
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
};

#endif // SAVE_STATE_H
