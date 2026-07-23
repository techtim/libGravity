#include "maps.h"
#include <EEPROM.h>
#include <Wire.h>
#include <libGravity.h>

// Configuration
const int MAX_DENSITY = 255;
const int MAP_RESOLUTION = 255;
const int MAX_CHAOS = 255;

// EEPROM addrs
const int EEPROM_INIT_ADDR = 0;
const int EEPROM_DENS_K = 1;
const int EEPROM_DENS_S = 3;
const int EEPROM_DENS_H = 5;
const int EEPROM_MAP_X = 7;
const int EEPROM_MAP_Y = 9;
const int EEPROM_CHAOS = 11;
const byte EEPROM_INIT_FLAG = 0xAC; // Update flag to re-init

// EEPROM Delay Save
const unsigned long SAVE_DELAY_MS = 5000;
bool eeprom_needs_save = false;
unsigned long last_param_change = 0;

// UI & Navigation
enum SelectedParam {
  PARAM_KICK_DENS = 0,
  PARAM_SNARE_DENS = 1,
  PARAM_HIHAT_DENS = 2,
  PARAM_CHAOS = 3,
  PARAM_MAP_X = 4,
  PARAM_MAP_Y = 5,
  PARAM_LAST = 6
};

SelectedParam current_param = PARAM_KICK_DENS;
bool editing_param = false;
bool needs_redraw = true;
unsigned long last_redraw = 0;
const unsigned long REDRAW_DELAY_MS = 30; // ~33fps limit

// Sequencer State
int current_step = 0;
bool is_playing = false;

// Engine Parameters (0-255)
int inst_density[3] = {128, 128,
                       128}; // Default 50% density for kick, snare, hihat
int map_x = 0;        // 0 to 255 (0 = House, 127 = Breakbeat, 255 = Hiphop)
int map_y = 127;      // 0 to 255 (0 = Sparse, 127 = Standard, 255 = Busy)
int chaos_amount = 0; // 0 to 255

volatile int cv1_val = 0;
volatile int cv2_val = 0;

// LFSR State for Chaos
uint16_t lfsr = 0xACE1;

// Math Helper: 1D Linear Interpolation between two bytes
uint8_t lerp(uint8_t a, uint8_t b, uint8_t t) {
  // t is 0-255. returns a if t=0, b if t=255
  return a + (((b - a) * t) >> 8);
}

// Math Helper: Get threshold from 2D map via interpolation
uint8_t GetThreshold(int inst, int step, int x_pos, int y_pos) {
  // x_pos is 0-255 mapped across 4 nodes (0, 1, 2, 3). Distance is 85 (255 / 3)
  // y_pos is 0-255 mapped across 4 nodes (0, 1, 2, 3). Distance is 85 (255 / 3)

  int x_idx = x_pos / 85; 
  int y_idx = y_pos / 85;

  uint8_t x_frac = (x_pos % 85) * 3; // scale remainder 0-84 up to 0-255
  uint8_t y_frac = (y_pos % 85) * 3;

  // Guard against out of bounds if exactly 255
  if (x_idx >= 3) {
    x_idx = 2;
    x_frac = 255;
  }
  if (y_idx >= 3) {
    y_idx = 2;
    y_frac = 255;
  }

  // Read 4 corners from PROGMEM
  uint8_t p00 = pgm_read_byte(&PATTERN_MAPS[x_idx][y_idx][inst][step]);
  uint8_t p10 = pgm_read_byte(&PATTERN_MAPS[x_idx + 1][y_idx][inst][step]);
  uint8_t p01 = pgm_read_byte(&PATTERN_MAPS[x_idx][y_idx + 1][inst][step]);
  uint8_t p11 = pgm_read_byte(&PATTERN_MAPS[x_idx + 1][y_idx + 1][inst][step]);

  // Bilinear interpolation
  uint8_t lerp_top = lerp(p00, p10, x_frac);
  uint8_t lerp_bottom = lerp(p01, p11, x_frac);
  return lerp(lerp_top, lerp_bottom, y_frac);
}

void LoadState() {
  if (EEPROM.read(EEPROM_INIT_ADDR) == EEPROM_INIT_FLAG) {
    EEPROM.get(EEPROM_DENS_K, inst_density[0]);
    EEPROM.get(EEPROM_DENS_S, inst_density[1]);
    EEPROM.get(EEPROM_DENS_H, inst_density[2]);
    EEPROM.get(EEPROM_MAP_X, map_x);
    EEPROM.get(EEPROM_MAP_Y, map_y);
    EEPROM.get(EEPROM_CHAOS, chaos_amount);
  }
}

void SaveState() {
  EEPROM.update(EEPROM_INIT_ADDR, EEPROM_INIT_FLAG);
  EEPROM.put(EEPROM_DENS_K, inst_density[0]);
  EEPROM.put(EEPROM_DENS_S, inst_density[1]);
  EEPROM.put(EEPROM_DENS_H, inst_density[2]);
  EEPROM.put(EEPROM_MAP_X, map_x);
  EEPROM.put(EEPROM_MAP_Y, map_y);
  EEPROM.put(EEPROM_CHAOS, chaos_amount);
}

// LFSR random bit generator (returns 0 or 1, fast)
uint8_t GetRandomBit() {
  uint8_t bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
  lfsr = (lfsr >> 1) | (bit << 15);
  return bit;
}

// Get 8-bit pseudo-random number
uint8_t GetRandomByte() {
  uint8_t r = 0;
  for (int i = 0; i < 8; i++) {
    r = (r << 1) | GetRandomBit();
  }
  return r;
}

void ProcessSequencerTick(uint32_t tick) {
  // Assuming 96 PPQN clock. We want 16th notes.
  // 96 pulses per quarter note / 4 = 24 pulses per 16th note.
  const int PULSES_PER_16TH = 24;

  // Pulse logic outputs low halfway through the 16th note (12 pulses)
  if (tick % PULSES_PER_16TH == 12) {
    for (int i = 0; i < 6; i++) {
      gravity.outputs[i].Low();
    }
  }

  // Handle new 16th note step
  if (tick % PULSES_PER_16TH == 0) {

    int mod_map_x = constrain(map_x + (cv1_val / 2), 0, 255);
    int active_chaos = constrain(chaos_amount + (cv2_val / 2), 0, 255);

    // Evaluate hits for Kick, Snare, HiHats
    for (int inst = 0; inst < 3; inst++) {
      uint8_t threshold = GetThreshold(inst, current_step, mod_map_x, map_y);
      int active_density = inst_density[inst];

      // Inject chaos
      if (active_chaos > 0) {
        // Chaos randomly adds or subtracts from density.
        int r = GetRandomByte();
        int chaos_variance = map(active_chaos, 0, 255, 0, 128);
        if (GetRandomBit()) {
          active_density += map(r, 0, 255, 0, chaos_variance);
        } else {
          active_density -= map(r, 0, 255, 0, chaos_variance);
        }
        active_density = constrain(active_density, 0, 255);
      }

      // Fire Trigger?
      if (active_density > threshold) {
        // Output 1-3
        gravity.outputs[inst].High();

        // Fire Accent Trigger? (If density greatly exceeds threshold)
        if (active_density > threshold + 60) {
          // Output 4-6
          gravity.outputs[inst + 3].High();
        }
      }
    }

    current_step = (current_step + 1) % 16;
  }
}

void OnPlayPress() {
  if (is_playing) {
    gravity.clock.Stop();
    for (int i = 0; i < 6; i++)
      gravity.outputs[i].Low();
  } else {
    gravity.clock.Start();
  }
  is_playing = !is_playing;
  needs_redraw = true;
}

void OnEncoderPress() {
  editing_param = !editing_param;
  needs_redraw = true;
}

void OnEncoderRotate(int val) {
  if (!editing_param) {
    // Navigate menu (clamp to edges, do not wrap)
    int next_param = (int)current_param + val;
    next_param = constrain(next_param, 0, PARAM_LAST - 1);
    current_param = (SelectedParam)next_param;
  } else {
    // Edit parameter
    int amt = val * 8; // Adjust by 8 values at a time for speed mapping

    switch (current_param) {
    case PARAM_KICK_DENS:
      inst_density[0] = constrain(inst_density[0] + amt, 0, 255);
      break;
    case PARAM_SNARE_DENS:
      inst_density[1] = constrain(inst_density[1] + amt, 0, 255);
      break;
    case PARAM_HIHAT_DENS:
      inst_density[2] = constrain(inst_density[2] + amt, 0, 255);
      break;
    case PARAM_CHAOS:
      chaos_amount = constrain(chaos_amount + amt, 0, 255);
      break;
    case PARAM_MAP_X:
      map_x = constrain(map_x + amt, 0, 255);
      break;
    case PARAM_MAP_Y:
      map_y = constrain(map_y + amt, 0, 255);
      break;
    default:
      break;
    }

    eeprom_needs_save = true;
    last_param_change = millis();
  }
  needs_redraw = true;
}

void DrawBarGraph(int y, const char *label, int value, bool is_selected) {
  // Reset draw color to default foreground
  gravity.display.setDrawColor(1);
  gravity.display.setCursor(0, y);

  if (is_selected) {
    gravity.display.print(">");
    if (editing_param) {
      // Draw solid white box behind the label
      gravity.display.drawBox(6, y - 8, 26, 10);
      // Switch to black text to 'cut out' the label from the box
      gravity.display.setDrawColor(0);
    }
  } else {
    gravity.display.print(" ");
  }

  gravity.display.setCursor(6, y);
  gravity.display.print(label);

  // Restore draw color to white for the bar and text
  gravity.display.setDrawColor(1);

  // Draw Bar
  int barLen = map(value, 0, 255, 0, 60);
  gravity.display.drawFrame(34, y - 8, 60, 8);
  gravity.display.drawBox(34, y - 8, barLen, 8);

  // Draw value percentage
  gravity.display.setCursor(98, y);
  int pct = map(value, 0, 255, 0, 100);
  gravity.display.print(pct);
  gravity.display.print("%");
}

void UpdateDisplay() {
  gravity.display.setFontMode(1);
  gravity.display.setDrawColor(1);
  gravity.display.setFont(u8g2_font_5x7_tf);

  // Header
  gravity.display.setCursor(0, 7);
  if (is_playing)
    gravity.display.print("[>] PLAY");
  else
    gravity.display.print("[||] PAUS");

  gravity.display.setCursor(55, 7);
  gravity.display.print("BPM:");
  gravity.display.print(gravity.clock.Tempo());

  gravity.display.drawHLine(0, 10, 128);

  // Parameters List (Scrollable window of 5 items)
  int y_start = 20;
  int y_spacing = 9;

  // Calculate window start index
  int window_start = max(0, min((int)current_param - 2, PARAM_LAST - 5));

  for (int i = 0; i < 5; i++) {
    int param_idx = window_start + i;
    int y_pos = y_start + (y_spacing * i);
    bool is_sel = (current_param == param_idx);

    switch (param_idx) {
    case PARAM_KICK_DENS:
      DrawBarGraph(y_pos, "KICK", inst_density[0], is_sel);
      break;
    case PARAM_SNARE_DENS:
      DrawBarGraph(y_pos, "SNAR", inst_density[1], is_sel);
      break;
    case PARAM_HIHAT_DENS:
      DrawBarGraph(y_pos, "HHAT", inst_density[2], is_sel);
      break;
    case PARAM_CHAOS:
      DrawBarGraph(y_pos, "CHAO", chaos_amount, is_sel);
      break;
    case PARAM_MAP_X:
      DrawBarGraph(y_pos, "MAPX", map_x, is_sel);
      break;
    case PARAM_MAP_Y:
      DrawBarGraph(y_pos, "MAPY", map_y, is_sel);
      break;
    }
  }
}

void setup() {
  gravity.Init();
  LoadState();

  gravity.play_button.AttachPressHandler(OnPlayPress);
  gravity.encoder.AttachPressHandler(OnEncoderPress);
  gravity.encoder.AttachRotateHandler(OnEncoderRotate);

  gravity.clock.AttachIntHandler(ProcessSequencerTick);
  // Default to 120 BPM internal
  gravity.clock.SetTempo(120);
  gravity.clock.SetSource(Clock::SOURCE_INTERNAL);

  // Speed up I2C for faster OLED refreshing
  Wire.setClock(400000);
}

void loop() {
  gravity.Process();

  // Apply CV modulation
  // CV1 modulates Map X, CV2 modulates Chaos
  cv1_val = gravity.cv1.Read(); // -512 to 512
  cv2_val = gravity.cv2.Read();

  if (eeprom_needs_save && (millis() - last_param_change > SAVE_DELAY_MS)) {
    SaveState();
    eeprom_needs_save = false;
  }

  if (needs_redraw && (millis() - last_redraw > REDRAW_DELAY_MS)) {
    needs_redraw = false;
    last_redraw = millis();
    gravity.display.firstPage();
    do {
      UpdateDisplay();
    } while (gravity.display.nextPage());
  }
}
