#include <EEPROM.h>
#include <libGravity.h>

// EEPROM addrs
const int EEPROM_INIT_ADDR = 0;
const int EEPROM_CV1_LOW = 1;
const int EEPROM_CV1_HIGH = 3;
const int EEPROM_CV1_OFFSET = 5;
const int EEPROM_CV2_LOW = 7;
const int EEPROM_CV2_HIGH = 9;
const int EEPROM_CV2_OFFSET = 11;
const int EEPROM_COMP1_SHIFT = 13;
const int EEPROM_COMP1_SIZE = 15;
const int EEPROM_COMP2_SHIFT = 17;
const int EEPROM_COMP2_SIZE = 19;
const int EEPROM_ENCODER_DIR = 21;
const int EEPROM_HYSTERESIS = 23;
const int EEPROM_DISPLAY_CV = 25;
const byte EEPROM_INIT_FLAG = 0xAB; // Update flag to re-init

// EEPROM Delay Save
const unsigned long SAVE_DELAY_MS = 5000;
bool eeprom_needs_save = false;
unsigned long last_param_change = 0;

enum AppMode { MODE_COMPARATOR, MODE_SETTINGS, MODE_CALIBRATION };
AppMode current_mode = MODE_COMPARATOR;
byte cal_selected_param = 0; // 0=CV1 Low, 1=CV1 Offset, 2=CV1 High, 3=CV2 Low,
                             // 4=CV2 Offset, 5=CV2 High

// Settings Menu State
bool encoder_reversed = false;
int hysteresis = 4;
bool display_cv = true;
bool settings_editing = false;

enum SettingsParameter { SETTING_ENCODER_DIR, SETTING_HYSTERESIS, SETTING_DISPLAY_CV };
SettingsParameter settings_selected_param = SETTING_ENCODER_DIR;

// UI Parameters
enum Parameter { COMP1_SHIFT, COMP1_SIZE, COMP2_SHIFT, COMP2_SIZE };

Parameter selected_param = COMP1_SHIFT;

int comp1_shift = 0;  // Range: -512 to 512
int comp1_size = 512; // Range: 0 to 1024

int comp2_shift = 0;  // Range: -512 to 512
int comp2_size = 512; // Range: 0 to 1024

bool prev_gate1 = false;
bool prev_gate2 = false;
bool ff_state = false;
bool needs_redraw = true;
int last_cv1_draw = -1000;
int last_cv2_draw = -1000;

unsigned long last_redraw = 0;

// Calibration Methods
void LoadCalibration() {
  if (EEPROM.read(EEPROM_INIT_ADDR) == EEPROM_INIT_FLAG) {
    int val = 0;
    EEPROM.get(EEPROM_CV1_LOW, val);
    gravity.cv1.SetCalibrationLow(val);
    EEPROM.get(EEPROM_CV1_HIGH, val);
    gravity.cv1.SetCalibrationHigh(val);
    EEPROM.get(EEPROM_CV1_OFFSET, val);
    gravity.cv1.AdjustOffset(val - gravity.cv1.GetOffset());
    EEPROM.get(EEPROM_CV2_LOW, val);
    gravity.cv2.SetCalibrationLow(val);
    EEPROM.get(EEPROM_CV2_HIGH, val);
    gravity.cv2.SetCalibrationHigh(val);
    EEPROM.get(EEPROM_CV2_OFFSET, val);
    gravity.cv2.AdjustOffset(val - gravity.cv2.GetOffset());

    EEPROM.get(EEPROM_COMP1_SHIFT, comp1_shift);
    EEPROM.get(EEPROM_COMP1_SIZE, comp1_size);
    EEPROM.get(EEPROM_COMP2_SHIFT, comp2_shift);
    EEPROM.get(EEPROM_COMP2_SIZE, comp2_size);

    EEPROM.get(EEPROM_ENCODER_DIR, encoder_reversed);
    EEPROM.get(EEPROM_HYSTERESIS, hysteresis);
    EEPROM.get(EEPROM_DISPLAY_CV, display_cv);
    if (hysteresis < 0 || hysteresis > 16) hysteresis = 4; // Bounds check
    gravity.encoder.SetReverseDirection(encoder_reversed);
  }
}

void SaveCalibration() {
  EEPROM.update(EEPROM_INIT_ADDR, EEPROM_INIT_FLAG);
  int val;
  val = gravity.cv1.GetCalibrationLow();
  EEPROM.put(EEPROM_CV1_LOW, val);
  val = gravity.cv1.GetCalibrationHigh();
  EEPROM.put(EEPROM_CV1_HIGH, val);
  val = gravity.cv1.GetOffset();
  EEPROM.put(EEPROM_CV1_OFFSET, val);
  val = gravity.cv2.GetCalibrationLow();
  EEPROM.put(EEPROM_CV2_LOW, val);
  val = gravity.cv2.GetCalibrationHigh();
  EEPROM.put(EEPROM_CV2_HIGH, val);
  val = gravity.cv2.GetOffset();
  EEPROM.put(EEPROM_CV2_OFFSET, val);

  EEPROM.put(EEPROM_COMP1_SHIFT, comp1_shift);
  EEPROM.put(EEPROM_COMP1_SIZE, comp1_size);
  EEPROM.put(EEPROM_COMP2_SHIFT, comp2_shift);
  EEPROM.put(EEPROM_COMP2_SIZE, comp2_size);

  EEPROM.put(EEPROM_ENCODER_DIR, encoder_reversed);
  EEPROM.put(EEPROM_HYSTERESIS, hysteresis);
  EEPROM.put(EEPROM_DISPLAY_CV, display_cv);
}

// Handlers
void OnEncoderPress() {
  if (current_mode == MODE_SETTINGS) {
    settings_editing = !settings_editing;
    needs_redraw = true;
  }
}

void HandlePressedRotate(int val) {
  int change = val > 0 ? 1 : -1;
  int next_mode = ((int)current_mode + change) % 3;
  if (next_mode < 0) next_mode += 3;

  // Cleanup current mode
  if (current_mode == MODE_SETTINGS) {
    settings_editing = false;
    SaveCalibration();
    gravity.encoder.SetReverseDirection(encoder_reversed);
  } else if (current_mode == MODE_CALIBRATION) {
    SaveCalibration();
  }

  current_mode = (AppMode)next_mode;

  // Setup new mode
  if (current_mode == MODE_CALIBRATION) {
    cal_selected_param = 0;
    // Turn off all outputs to prevent phantom gates while tuning
    for (int i = 0; i < 6; i++) {
      gravity.outputs[i].Update(LOW);
    }
  }

  needs_redraw = true;
}

void OnPlayPress() {
  if (gravity.shift_button.On())
    return; // ignore if holding both
  if (current_mode == MODE_SETTINGS) return; // Ignore in settings
  if (current_mode == MODE_CALIBRATION) {
    cal_selected_param = (cal_selected_param < 3) ? cal_selected_param + 3
                                                  : cal_selected_param - 3;
    needs_redraw = true;
    return;
  }
  if (selected_param == COMP1_SHIFT)
    selected_param = COMP2_SHIFT;
  else if (selected_param == COMP1_SIZE)
    selected_param = COMP2_SIZE;
  else if (selected_param == COMP2_SHIFT)
    selected_param = COMP1_SHIFT;
  else if (selected_param == COMP2_SIZE)
    selected_param = COMP1_SIZE;
  needs_redraw = true;
}

void OnShiftPress() {
  if (gravity.play_button.On())
    return; // ignore if holding both
  if (current_mode == MODE_SETTINGS) return; // Ignore in settings
  if (current_mode == MODE_CALIBRATION) {
    cal_selected_param =
        (cal_selected_param / 3) * 3 + ((cal_selected_param + 1) % 3);
    needs_redraw = true;
    return;
  }
  if (selected_param == COMP1_SHIFT)
    selected_param = COMP1_SIZE;
  else if (selected_param == COMP1_SIZE)
    selected_param = COMP1_SHIFT;
  else if (selected_param == COMP2_SHIFT)
    selected_param = COMP2_SIZE;
  else if (selected_param == COMP2_SIZE)
    selected_param = COMP2_SHIFT;
  needs_redraw = true;
}

void OnEncoderRotate(int val) {
  if (current_mode == MODE_SETTINGS) {
    if (!settings_editing) {
      // Navigate Mode
      int change = val > 0 ? 1 : -1;
      settings_selected_param = (SettingsParameter)(((int)settings_selected_param + change + 3) % 3);
    } else {
      // Edit Mode
      switch (settings_selected_param) {
        case SETTING_ENCODER_DIR: {
          int setting = encoder_reversed ? 1 : 0;
          setting = constrain(setting + (val > 0 ? 1 : -1), 0, 1);
          encoder_reversed = (setting == 1);
          gravity.encoder.SetReverseDirection(encoder_reversed);
          break;
        }
        case SETTING_HYSTERESIS:
          hysteresis = constrain(hysteresis + val, 0, 16);
          break;
        case SETTING_DISPLAY_CV: {
          int setting = display_cv ? 1 : 0;
          setting = constrain(setting + (val > 0 ? 1 : -1), 0, 1);
          display_cv = (setting == 1);
          break;
        }
      }
      eeprom_needs_save = true;
      last_param_change = millis();
    }
    needs_redraw = true;
    return;
  }

  if (current_mode == MODE_CALIBRATION) {
    AnalogInput *cv = (cal_selected_param > 2) ? &gravity.cv2 : &gravity.cv1;
    // Scale val up so tuning is practical without excessive encoder interrupts
    int cal_adj = val * 8;
    switch (cal_selected_param % 3) {
    case 0:
      cv->AdjustCalibrationLow(cal_adj);
      break;
    case 1:
      cv->AdjustOffset(cal_adj);
      break;
    case 2:
      cv->AdjustCalibrationHigh(cal_adj);
      break;
    }
    needs_redraw = true;
    return;
  }

  int amount = val * 16;
  switch (selected_param) {
  case COMP1_SHIFT:
    comp1_shift = constrain(comp1_shift + amount, -512, 512);
    break;
  case COMP1_SIZE:
    comp1_size = constrain(comp1_size + amount, 0, 1024);
    break;
  case COMP2_SHIFT:
    comp2_shift = constrain(comp2_shift + amount, -512, 512);
    break;
  case COMP2_SIZE:
    comp2_size = constrain(comp2_size + amount, 0, 1024);
    break;
  }

  eeprom_needs_save = true;
  last_param_change = millis();
  needs_redraw = true;
}

void DisplayCalibrationPoint(AnalogInput *cv, const char *title, int index) {
  int barWidth = 100, barHeight = 10, textHeight = 10;
  int half = barWidth / 2;
  int offsetX = 16, offsetY = (32 * index);

  gravity.display.setDrawColor(1);
  int value = cv->Read();

  gravity.display.setCursor(0, offsetY + textHeight);
  gravity.display.print(title);
  if (value >= 0)
    gravity.display.print(" ");
  gravity.display.print(value);

  gravity.display.setCursor(92, offsetY + textHeight);
  if (cv->Voltage() >= 0)
    gravity.display.print(" ");
  gravity.display.print(cv->Voltage(), 1);
  gravity.display.print(F("V"));

  gravity.display.drawFrame(offsetX, textHeight + offsetY + 2, barWidth,
                            barHeight);
  if (value > 0) {
    int x = constrain(map(value, 0, 512, 0, half), 0, half);
    gravity.display.drawBox(half + offsetX, textHeight + offsetY + 2, x,
                            barHeight);
  } else {
    int x = constrain(map(abs(value), 0, 512, 0, half), 0, half);
    gravity.display.drawBox((half + offsetX) - x, textHeight + offsetY + 2, x,
                            barHeight);
  }

  if (cal_selected_param / 3 == index) {
    int left = offsetX + (half * (cal_selected_param % 3) - 2);
    int top = barHeight + textHeight + offsetY + 12;
    gravity.display.drawStr(left, top, "^");
  }
}

void UpdateCalibrationDisplay() {
  gravity.display.setFontMode(0);
  gravity.display.setDrawColor(1);
  gravity.display.setFont(u8g2_font_profont11_tf);
  DisplayCalibrationPoint(&gravity.cv1, "CV1: ", 0);
  DisplayCalibrationPoint(&gravity.cv2, "CV2: ", 1);
}

void UpdateSettingsDisplay() {
  gravity.display.setFontMode(0);
  gravity.display.setDrawColor(1);
  gravity.display.setFont(u8g2_font_profont11_tf);
  
  gravity.display.setCursor(0, 10);
  gravity.display.print("Settings:");

  int y = 25;
  gravity.display.setCursor(10, y);
  gravity.display.print("Encoder:");
  gravity.display.setCursor(80, y);
  gravity.display.print(encoder_reversed ? "Rev" : "Fwd");

  y += 12;
  gravity.display.setCursor(10, y);
  gravity.display.print("Hysteresis:");
  gravity.display.setCursor(80, y);
  gravity.display.print(hysteresis);

  y += 12;
  gravity.display.setCursor(10, y);
  gravity.display.print("Display CV:");
  gravity.display.setCursor(80, y);
  gravity.display.print(display_cv ? "On" : "Off");

  // Selection indicator cursor `>` or `*`
  y = 25 + (12 * (int)settings_selected_param);
  gravity.display.setCursor(0, y);
  gravity.display.print(settings_editing ? "*" : ">");
}

// UpdateDisplay
void UpdateDisplay(int cv1_val, int cv2_val) {
  // Comp 1 graphics (Left)
  int c1_h = max((comp1_size * 3) / 64, 1);
  int c1_center = 26 - ((comp1_shift * 3) / 64);
  int c1_y = c1_center - (c1_h / 2);
  gravity.display.drawFrame(20, c1_y, 44, c1_h);

  // CV 1 Indicator (Filled Box, 50% width, from 0V center)
  if (display_cv) {
    int cv1_y = constrain(26 - ((cv1_val * 3) / 64), 2, 50);
    if (cv1_val >= 0) {
      gravity.display.drawBox(31, cv1_y, 22, 26 - cv1_y);
    } else {
      gravity.display.drawBox(31, 26, 22, cv1_y - 26);
    }
  }

  // Comp 2 graphics (Right)
  int c2_h = max((comp2_size * 3) / 64, 1);
  int c2_center = 26 - ((comp2_shift * 3) / 64);
  int c2_y = c2_center - (c2_h / 2);
  gravity.display.drawFrame(74, c2_y, 44, c2_h);

  // CV 2 Indicator (Filled Box, 50% width, from 0V center)
  if (display_cv) {
    int cv2_y = constrain(26 - ((cv2_val * 3) / 64), 2, 50);
    if (cv2_val >= 0) {
      gravity.display.drawBox(85, cv2_y, 22, 26 - cv2_y);
    } else {
      gravity.display.drawBox(85, 26, 22, cv2_y - 26);
    }
  }

  // Restore solid drawing for labels
  gravity.display.setDrawColor(1);
  gravity.display.setFont(u8g2_font_5x7_tf);
  gravity.display.setCursor(0, 7);
  gravity.display.print("+5V");
  gravity.display.setCursor(6, 29);
  gravity.display.print("0V");
  gravity.display.setCursor(0, 51);
  gravity.display.print("-5V");

  gravity.display.setDrawColor(2); // XOR mode

  // Draw center divider and dotted lines in XOR
  for (int x = 20; x < 128; x += 4) {
    gravity.display.drawPixel(x, 2);  // +5V
    gravity.display.drawPixel(x, 26); // 0V
    gravity.display.drawPixel(x, 50); // -5V
  }
  for (int y = 0; y <= 50; y += 4) {
    gravity.display.drawPixel(69, y); // Center divider
  }

  // Restore draw color to default (solid)
  gravity.display.setDrawColor(1);

  // Bottom text area
  gravity.display.setDrawColor(0);
  gravity.display.drawBox(0, 52, 128, 12);
  gravity.display.setDrawColor(1);
  gravity.display.drawHLine(0, 52, 128);

  gravity.display.setFont(u8g2_font_6x10_tf);
  gravity.display.setCursor(2, 62);

  char text[32];
  switch (selected_param) {
  case COMP1_SHIFT:
    snprintf(text, sizeof(text), "> Comp 1 Shift: %d", comp1_shift);
    break;
  case COMP1_SIZE:
    snprintf(text, sizeof(text), "> Comp 1 Size: %d", comp1_size);
    break;
  case COMP2_SHIFT:
    snprintf(text, sizeof(text), "> Comp 2 Shift: %d", comp2_shift);
    break;
  case COMP2_SIZE:
    snprintf(text, sizeof(text), "> Comp 2 Size: %d", comp2_size);
    break;
  }
  gravity.display.print(text);
}

void setup() {
  gravity.Init();
  LoadCalibration();

  // Speed up ADC conversions
  ADCSRA &= ~(bit(ADPS2) | bit(ADPS1) | bit(ADPS0));
  ADCSRA |= bit(ADPS2);

  gravity.play_button.AttachPressHandler(OnPlayPress);
  gravity.shift_button.AttachPressHandler(OnShiftPress);
  gravity.encoder.AttachPressHandler(OnEncoderPress);
  gravity.encoder.AttachRotateHandler(OnEncoderRotate);
  gravity.encoder.AttachPressRotateHandler(HandlePressedRotate);
}

void loop() {
  gravity.Process();

  int cv1_val = gravity.cv1.Read();
  int cv2_val = gravity.cv2.Read();

  if (current_mode == MODE_COMPARATOR) {
    int c1_lower = comp1_shift - (comp1_size / 2);
    int c1_upper = comp1_shift + (comp1_size / 2);

    int c2_lower = comp2_shift - (comp2_size / 2);
    int c2_upper = comp2_shift + (comp2_size / 2);

    bool gate1 = prev_gate1;
    if (gate1) {
      if (cv1_val < c1_lower - hysteresis || cv1_val > c1_upper + hysteresis) {
        gate1 = false;
      }
    } else {
      if (cv1_val >= c1_lower + hysteresis &&
          cv1_val <= c1_upper - hysteresis) {
        gate1 = true;
      }
    }

    bool gate2 = prev_gate2;
    if (gate2) {
      if (cv2_val < c2_lower - hysteresis || cv2_val > c2_upper + hysteresis) {
        gate2 = false;
      }
    } else {
      if (cv2_val >= c2_lower + hysteresis &&
          cv2_val <= c2_upper - hysteresis) {
        gate2 = true;
      }
    }

    bool logic_and = gate1 && gate2;
    bool logic_or = gate1 || gate2;
    bool logic_xor = gate1 ^ gate2;

    static bool prev_logic_xor = false;
    if (logic_xor && !prev_logic_xor) {
      ff_state = !ff_state;
    }
    prev_logic_xor = logic_xor;

    gravity.outputs[0].Update(gate1 ? HIGH : LOW);
    gravity.outputs[1].Update(gate2 ? HIGH : LOW);
    gravity.outputs[2].Update(logic_and ? HIGH : LOW);
    gravity.outputs[3].Update(logic_or ? HIGH : LOW);
    gravity.outputs[4].Update(logic_xor ? HIGH : LOW);
    gravity.outputs[5].Update(ff_state ? HIGH : LOW);

    prev_gate1 = gate1;
    prev_gate2 = gate2;
  }

  if (eeprom_needs_save && (millis() - last_param_change > SAVE_DELAY_MS)) {
    SaveCalibration();
    eeprom_needs_save = false;
  }

  if (current_mode == MODE_COMPARATOR) {
    if (display_cv) {
      if (abs(cv1_val - last_cv1_draw) > 12 ||
          abs(cv2_val - last_cv2_draw) > 12) {
        needs_redraw = true;
      }
    } else if (abs(comp1_shift - last_cv1_draw /* hack to force redraw on comp change if CV disabled? */) ) {
        // We only trigger redraws if comp1/comp2 settings change. But those set needs_redraw=true in OnEncoderRotate. 
        // So if display_cv is false, we don't redraw from CV fluctuations at all loop.
    }
  } else if (current_mode == MODE_CALIBRATION) {
    // Need frequent redraws in calibration to see the live target input
    if (abs(cv1_val - last_cv1_draw) >= 2 ||
        abs(cv2_val - last_cv2_draw) >= 2) {
      needs_redraw = true;
    }
  }

  // Unroll the display loop so it doesn't block the logic loop
  static bool is_drawing = false;

  if (needs_redraw && !is_drawing && (millis() - last_redraw >= 30)) {
    needs_redraw = false;
    is_drawing = true;
    last_redraw = millis();
    gravity.display.firstPage();
  }

  if (is_drawing) {
    if (current_mode == MODE_COMPARATOR) {
      last_cv1_draw = cv1_val;
      last_cv2_draw = cv2_val;
      UpdateDisplay(cv1_val, cv2_val);
    } else if (current_mode == MODE_CALIBRATION) {
      UpdateCalibrationDisplay();
    } else if (current_mode == MODE_SETTINGS) {
      UpdateSettingsDisplay();
    }
    is_drawing = gravity.display.nextPage();
  }
}