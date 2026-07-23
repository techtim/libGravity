/**
 * @file save_state.h
 * @author Adam Wonak (https://github.com/awonak/)
 * @brief Alt firmware version of Gravity by Sitka Instruments.
 * @version 2.0.2
 * @date 2025-07-04
 *
 * @copyright MIT - (c) 2025 - Adam Wonak - adam.wonak@gmail.com
 *
 */

#ifndef SAVE_STATE_H
#define SAVE_STATE_H

#include <Arduino.h>
#include <libGravity.h>

// Forward-declare AppState to avoid circular dependencies.
struct AppState;

/**
 * @brief Manages saving and loading of the application state to and from
 * EEPROM. The number of user slots is defined by MAX_SAVE_SLOTS, and one
 * additional slot is reseved for transient state to persist state between power
 * cycles before state is explicitly saved to a user slot. Metadata is stored in
 * the beginning of the memory space which stores firmware version information
 * to validate that the data can be loaded into the current version of AppState.
 */
class StateManager {
public:
  static const char SKETCH_NAME[];
  static const char SEMANTIC_VERSION[];
  static const byte MAX_SAVE_SLOTS;
  static const byte TRANSIENT_SLOT;

  StateManager();

  // Populate the AppState instance with values from EEPROM if they exist.
  bool initialize(AppState &app);
  // Load data from specified slot.
  bool loadData(AppState &app, byte slot_index);
  // Save data to specified slot.
  void saveData(const AppState &app);
  // Reset AppState instance back to default values.
  void reset(AppState &app);
  // Call from main loop, check if state has changed and needs to be saved.
  void update(const AppState &app);
  // Indicate that state has changed and we should save.
  void markDirty();
  // Erase all data stored in the EEPROM.
  void factoryReset(AppState &app);

  // This struct holds the data that identifies the firmware version.
  struct Metadata {
    char sketch_name[16];
    char version[16];
    // Additional global/hardware settings
    byte selected_save_slot;
    bool encoder_reversed;
    bool rotate_display;
  };
  struct ChannelState {
    byte base_clock_mod_index;
    byte base_probability;
    byte base_duty_cycle;
    byte base_offset;
    byte cv1_dest; // Cast the CvDestination enum as a byte for storage
    byte cv2_dest; // Cast the CvDestination enum as a byte for storage
  };
  // This struct holds all the parameters we want to save.
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

  bool _isDirty;
  unsigned long _lastChangeTime;
};

#endif // SAVE_STATE_H