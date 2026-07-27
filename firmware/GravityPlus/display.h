/**
 * @file display.h
 * @brief OLED UI for GravityPlus. The global page is unchanged from the shared
 *        Gravity UI; the channel page renders the fixed clock-mod + six gate
 *        params + two CV targets.
 */
#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

#include "app_state.h"
#include "save_state.h"

//
// UI Display functions for drawing the UI to the OLED display.
//

/*
 * Font: velvetscreen.bdf 9pt
 * https://stncrn.github.io/u8g2-unifont-helper/
 * "%/0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
 */
const uint8_t TEXT_FONT[437] U8G2_FONT_SECTION("velvetscreen") PROGMEM =
    "\64\0\2\2\3\3\2\3\4\5\5\0\0\5\0\5\0\0\221\0\0\1\230 \4\200\134%\11\255tT"
    "R\271RI(\6\252\334T\31)\7\252\134bJ\12+\7\233\345\322J\0,\5\221T\4-\5\213"
    "f\6.\5\211T\2/"
    "\6\244\354c\33\60\10\254\354T\64\223\2\61\7\353\354\222\254\6\62\11\254l"
    "\66J*"
    "\217\0\63\11\254l\66J\32\215\4\64\10\254l\242\34\272\0\65\11\254l\206\336h"
    "$\0\66"
    "\11\254\354T^\61)\0\67\10\254lF\216u\4\70\11\254\354TL*&"
    "\5\71\11\254\354TL;"
    ")\0:\6\231UR\0A\10\254\354T\34S\6B\11\254lV\34)\216\4C\11\254\354T\324\61"
    ")\0D\10\254lV\64G\2E\10\254l\206\36z\4F\10\254l\206^\71\3G\11\254\354TN"
    "\63)"
    "\0H\10\254l\242\34S\6I\6\251T\206\0J\10\254\354k\231\24\0K\11\254l\242J\62"
    "\225\1L\7\254lr{\4M\11\255t\362ZI\353\0N\11\255t\362TI\356\0O\10\254\354T"
    "\64\223\2P\11\254lV\34)"
    "g\0Q\10\254\354T\264b\12R\10\254lV\34\251\31S\11\254\354"
    "FF\32\215\4T\7\253dVl\1U\10\254l\242\63)\0V\11\255t\262Ne\312\21W\12\255"
    "t\262J*\251.\0X\11\254l\242L*\312\0Y\12\255tr\252\63\312(\2Z\7\253df*"
    "\7p\10\255\364V\266\323\2q\7\255\364\216\257\5r\10\253d\242\32*"
    "\2t\6\255t\376#w\11"
    "\255\364V\245FN\13x\6\233dR\7\0\0\0\4\377\377\0";

/*
 * Font: STK-L.bdf 36pt
 * https://stncrn.github.io/u8g2-unifont-helper/
 * "%/0123456789ABCDEFILNORSTUVXx"
 */
const uint8_t LARGE_FONT[766] U8G2_FONT_SECTION("stk-l") =
    "\35\0\4\4\4\5\3\1\6\20\30\0\0\27\0\0\0\1\77\0\0\2\341%'\17;\226\261\245FL"
    "\64B\214\30\22\223\220)"
    "Bj\10Q\232\214\42R\206\310\210\21d\304\30\32a\254\304\270!\0/\14"
    "\272\272\275\311H\321g\343\306\1\60\37|\373\35CJT\20:"
    "fW\207\320\210\60\42\304\204\30D\247"
    "\214\331\354\20\11%"
    "\212\314\0\61\24z\275\245a\244\12\231\71\63b\214\220q\363\377(E\6\62\33|"
    "\373\35ShT\20:fl\344\14\211\231\301\306T\71\202#g\371\340\201\1\63\34|"
    "\373\35ShT"
    "\20:fl\344@r\264\263\222\344,\215\35\42\241\6\225\31\0\64 "
    "|\373-!\203\206\214!\62\204"
    "\314\220A#\10\215\30\65b\324\210Q\306\354\354\1\213\225\363\1\65\32|"
    "\373\15\25[\214\234/\10)"
    "Y\61j\350\310Y\32;DB\15*\63\0\66\33}\33\236SiV\14;gt^\230Y\302\202\324"
    "\71\273;EbM\252\63\0\67\23|\373\205\25\17R\316\207\344\350p\312\201#"
    "\347\35\0\70 |\373"
    "\35ShT\20:f\331!\22D\310 "
    ":\205\206\10\11B\307\354\354\20\11\65\250\314\0\71\32|\373"
    "\35ShT\20:fg\207H,Q\223r\276\30DB\15*\63\0A\26}\33\246r\247\322P\62"
    "j\310\250\21\343\354\335\203\357\354w\3B$}"
    "\33\206Dj\226\214\42\61l\304\260\21\303F\14\33\61"
    "\212\304\222MF\221\30v\316\236=\10\301b\11\0C\27}"
    "\33\236Si\226\20Bft\376O\211\215"
    " Db\215\42$\0D\33}\33\206Dj\226\214\32\62l\304\260\21\343\354\177vl\304("
    "\22K\324"
    "$\2E\22|\373\205\17R\316KD\30\215\234_>x`\0F\20|"
    "\373\205\17R\316\227i\262\31"
    "\71\377\22\0I\7s\333\204\77HL\15{\333\205\201\363\377\77|\360`\0N$}"
    "\33\6\201\346\314"
    "\35;\206\12U\242D&\306\230\30cd\210\221!fF\230\31a(+\314\256\63\67\0O\26}"
    "\33"
    "\236Si\226\214\32\61\316\376\277\33\61j\310\232Tg\0R\61\216;\6Ek\230\14#"
    "\61n\304\270"
    "\21\343F\214\33\61n\304\60\22\243\210\60Q\224j\310\260\61\243\306\20\232"
    "\325\230QD\206\221\30\67b"
    "\334\301\1S\42\216;\236c\211\226\220\42\61n\304\270\21c\307R\232,["
    "\262\203\307\216\65h\16\25"
    "\21&\253\320\0T\15}\33\206\17R\15\235\377\377\25\0U\21|"
    "\373\205a\366\377\237\215\30\64D\15"
    "*\63\0V\26\177\371\205\221\366\377\313\21\343\206\220\42C\25\11r'"
    "\313\16\3X)~;\206\201\6"
    "\217\221\30\66\204\20\31\42\244\206\14Cg\320$Q\222\6\315!"
    "\33\62\212\10\31BD\206\215 v\320"
    "\302\1x\24\312\272\205A\206\216\220@c\212\224\31$"
    "S\14\262h\0\0\0\0\4\377\377\0";

#define play_icon_width 14
#define play_icon_height 14
static const unsigned char play_icon[28] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x7C, 0x00, 0xFC, 0x00,
    0xFC, 0x03, 0xFC, 0x0F, 0xFC, 0x0F, 0xFC, 0x03, 0xFC, 0x00,
    0x7C, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00};
static const unsigned char pause_icon[28] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x38, 0x0E, 0x38, 0x0E, 0x38, 0x0E,
    0x38, 0x0E, 0x38, 0x0E, 0x38, 0x0E, 0x38, 0x0E, 0x38, 0x0E,
    0x38, 0x0E, 0x38, 0x0E, 0x38, 0x0E, 0x00, 0x00};

// Constants for screen layout and fonts
constexpr uint8_t SCREEN_CENTER_X = 32;
constexpr uint8_t MAIN_TEXT_Y = 34;
constexpr uint8_t SUB_TEXT_Y = 44;
constexpr uint8_t VISIBLE_MENU_ITEMS = 3;
constexpr uint8_t MENU_ITEM_Y = 4;
constexpr uint8_t MENU_ITEM_HEIGHT = 14;
constexpr uint8_t MENU_BOX_PADDING = 4;
constexpr uint8_t MENU_BOX_WIDTH = 64;
constexpr uint8_t CHANNEL_BOXES_Y = 50;
constexpr uint8_t CHANNEL_BOX_WIDTH = 18;
constexpr uint8_t CHANNEL_BOX_HEIGHT = 14;

// Menu items for editing global parameters.
enum ParamsMainPage : uint8_t {
  PARAM_MAIN_TEMPO,
  PARAM_MAIN_RUN,
  PARAM_MAIN_RESET,
  PARAM_MAIN_SOURCE,
  PARAM_MAIN_PULSE,
  PARAM_MAIN_ENCODER_DIR,
  PARAM_MAIN_ROTATE_DISP,
  PARAM_MAIN_SAVE_DATA,
  PARAM_MAIN_LOAD_DATA,
  PARAM_MAIN_RESET_STATE,
  // CV calibration sits just above ERASE.
  PARAM_MAIN_CV1_CAL_LO,
  PARAM_MAIN_CV1_CAL_ZERO,
  PARAM_MAIN_CV1_CAL_HI,
  PARAM_MAIN_CV2_CAL_LO,
  PARAM_MAIN_CV2_CAL_ZERO,
  PARAM_MAIN_CV2_CAL_HI,
  PARAM_MAIN_FACTORY_RESET,
  PARAM_MAIN_LAST,
};

// Scratch text buffers. We avoid the Arduino String class here: on AVR it pulls
// in operator+, number formatting and the heap (~1.5KB of flash). Building the
// two on-screen lines into fixed buffers keeps the firmware within flash.
static char g_main[16];
static char g_sub[20];

// Copy a flash (PROGMEM) string into a RAM buffer for u8g2 text functions.
inline void copyP(char *dst, size_t n, const __FlashStringHelper *f) {
  strncpy_P(dst, reinterpret_cast<PGM_P>(f), n - 1);
  dst[n - 1] = '\0';
}

// Helper function to draw centered text
void drawCenteredText(const char *text, int y, const uint8_t *font) {
  gravity.display.setFont(font);
  int textWidth = gravity.display.getStrWidth(text);
  gravity.display.drawStr(SCREEN_CENTER_X - (textWidth / 2), y, text);
}

// Helper function to draw right-aligned text
void drawRightAlignedText(const char *text, int y) {
  int textWidth = gravity.display.getStrWidth(text);
  int drawX = (SCREEN_WIDTH - textWidth) - MENU_BOX_PADDING;
  gravity.display.drawStr(drawX, y, text);
}

void drawMainSelection() {
  gravity.display.setDrawColor(1);
  const int offsetY = 5;
  const int tickSize = 3;
  const int mainWidth = SCREEN_WIDTH / 2;
  const int mainHeight = 46;
  gravity.display.drawLine(0, offsetY, tickSize, offsetY);
  gravity.display.drawLine(0, offsetY, 0, tickSize + offsetY);
  gravity.display.drawLine(mainWidth, offsetY, mainWidth - tickSize, offsetY);
  gravity.display.drawLine(mainWidth, offsetY, mainWidth, tickSize + offsetY);
  gravity.display.drawLine(mainWidth, mainHeight, mainWidth,
                           mainHeight - tickSize);
  gravity.display.drawLine(mainWidth, mainHeight, mainWidth - tickSize,
                           mainHeight);
  gravity.display.drawLine(0, mainHeight, tickSize, mainHeight);
  gravity.display.drawLine(0, mainHeight, 0, mainHeight - tickSize);
  gravity.display.setDrawColor(2);
}

void drawMenuItems(const __FlashStringHelper *menu_items[], int menu_size) {
  gravity.display.setFont(TEXT_FONT);

  // Draw selected menu item box
  int selectedBoxY = 0;
  if (menu_size >= VISIBLE_MENU_ITEMS && app.selected_param == menu_size - 1) {
    selectedBoxY = MENU_ITEM_HEIGHT * min(2, app.selected_param);
  } else if (app.selected_param > 0) {
    selectedBoxY = MENU_ITEM_HEIGHT;
  }

  int boxX = MENU_BOX_WIDTH + 1;
  int boxY = MENU_ITEM_Y + selectedBoxY + 2;
  int boxWidth = MENU_BOX_WIDTH - 1;
  int boxHeight = MENU_ITEM_HEIGHT + 1;

  if (app.editing_param) {
    gravity.display.drawBox(boxX, boxY, boxWidth, boxHeight);
    drawMainSelection();
  } else {
    gravity.display.drawFrame(boxX, boxY, boxWidth, boxHeight);
  }

  // Draw the visible menu items
  int start_index = 0;
  if (menu_size >= VISIBLE_MENU_ITEMS && app.selected_param == menu_size - 1) {
    start_index = menu_size - VISIBLE_MENU_ITEMS;
  } else if (app.selected_param > 0) {
    start_index = app.selected_param - 1;
  }

  for (uint8_t i = 0; i < min(menu_size, VISIBLE_MENU_ITEMS); ++i) {
    int idx = start_index + i;
    copyP(g_sub, sizeof(g_sub), menu_items[idx]);
    drawRightAlignedText(g_sub, MENU_ITEM_Y + MENU_ITEM_HEIGHT * (i + 1) - 1);
  }
}

// Visual indicator: mark the active save slot.
inline void solidTick() { gravity.display.drawBox(56, 4, 4, 4); }

// Center-zero horizontal bar meter for a bipolar CV reading (-512..+512). The
// fill grows right of centre for positive readings, left for negative.
void drawCvMeter(int value, int x, int y, int w, int h) {
  const int half = w / 2;
  const int cx = x + half;
  gravity.display.setDrawColor(1);
  gravity.display.drawFrame(x, y, w, h);
  gravity.display.drawVLine(cx, y - 2, h + 4); // centre tick
  if (value >= 0) {
    int fill = constrain(map(value, 0, 512, 0, half), 0, half);
    if (fill > 0)
      gravity.display.drawBox(cx, y, fill, h);
  } else {
    int fill = constrain(map(-value, 0, 512, 0, half), 0, half);
    if (fill > 0)
      gravity.display.drawBox(cx - fill, y, fill, h);
  }
  gravity.display.setDrawColor(2);
}

// Human friendly display value for save slot, written into `out` (e.g. "A3").
void displaySaveSlot(char *out, int slot) {
  const int half = StateManager::MAX_SAVE_SLOTS / 2;
  if (slot < half) {
    out[0] = 'A';
    itoa(slot + 1, out + 1, 10);
  } else {
    out[0] = 'B';
    itoa(slot - half + 1, out + 1, 10);
  }
}

// Main (global) settings page.
void DisplayMainPage() {
  gravity.display.setFontMode(1);
  gravity.display.setDrawColor(2);
  gravity.display.setFont(TEXT_FONT);

  g_main[0] = '\0';
  g_sub[0] = '\0';
  // The CV range params draw a live signal meter in place of the big value.
  bool show_cv_meter = false;
  int cv_meter_value = 0;

  switch (app.selected_param) {
  case PARAM_MAIN_TEMPO:
    // Serial MIDI is too unstable to display bpm in real time.
    if (app.selected_source == Clock::SOURCE_EXTERNAL_MIDI) {
      copyP(g_main, sizeof(g_main), F("EXT"));
    } else {
      itoa(gravity.clock.Tempo(), g_main, 10);
    }
    copyP(g_sub, sizeof(g_sub), F("BPM"));
    break;
  case PARAM_MAIN_RUN:
    copyP(g_main, sizeof(g_main), F("RUN"));
    switch (app.cv_run) {
    case 0: copyP(g_sub, sizeof(g_sub), F("NONE")); break;
    case 1: copyP(g_sub, sizeof(g_sub), F("CV1 GATE")); break;
    case 2: copyP(g_sub, sizeof(g_sub), F("CV2 GATE")); break;
    }
    break;
  case PARAM_MAIN_RESET:
    copyP(g_main, sizeof(g_main), F("RST"));
    switch (app.cv_reset) {
    case CV_RESET_NONE: copyP(g_sub, sizeof(g_sub), F("NONE")); break;
    case CV_RESET_CV1: copyP(g_sub, sizeof(g_sub), F("CV1 TRIG")); break;
    case CV_RESET_CV2: copyP(g_sub, sizeof(g_sub), F("CV2 TRIG")); break;
    case CV_RESET_EXT: copyP(g_sub, sizeof(g_sub), F("EXT TRIG")); break;
    }
    break;
  case PARAM_MAIN_CV1_CAL_LO:
  case PARAM_MAIN_CV1_CAL_ZERO:
  case PARAM_MAIN_CV1_CAL_HI:
  case PARAM_MAIN_CV2_CAL_LO:
  case PARAM_MAIN_CV2_CAL_ZERO:
  case PARAM_MAIN_CV2_CAL_HI: {
    bool is1 = app.selected_param <= PARAM_MAIN_CV1_CAL_HI;
    cv_meter_value = is1 ? gravity.cv1.Read() : gravity.cv2.Read();
    show_cv_meter = true; // tune against the live reading
    switch (app.selected_param) {
    case PARAM_MAIN_CV1_CAL_LO: copyP(g_sub, sizeof(g_sub), F("CV1 CAL -5V")); break;
    case PARAM_MAIN_CV1_CAL_ZERO: copyP(g_sub, sizeof(g_sub), F("CV1 CAL 0V")); break;
    case PARAM_MAIN_CV1_CAL_HI: copyP(g_sub, sizeof(g_sub), F("CV1 CAL +5V")); break;
    case PARAM_MAIN_CV2_CAL_LO: copyP(g_sub, sizeof(g_sub), F("CV2 CAL -5V")); break;
    case PARAM_MAIN_CV2_CAL_ZERO: copyP(g_sub, sizeof(g_sub), F("CV2 CAL 0V")); break;
    default: copyP(g_sub, sizeof(g_sub), F("CV2 CAL +5V")); break;
    }
    break;
  }
  case PARAM_MAIN_SOURCE:
    copyP(g_main, sizeof(g_main), F("EXT"));
    switch (app.selected_source) {
    case Clock::SOURCE_INTERNAL:
      copyP(g_main, sizeof(g_main), F("INT"));
      copyP(g_sub, sizeof(g_sub), F("CLOCK"));
      break;
    case Clock::SOURCE_EXTERNAL_PPQN_24: copyP(g_sub, sizeof(g_sub), F("24 PPQN")); break;
    case Clock::SOURCE_EXTERNAL_PPQN_4: copyP(g_sub, sizeof(g_sub), F("4 PPQN")); break;
    case Clock::SOURCE_EXTERNAL_PPQN_2: copyP(g_sub, sizeof(g_sub), F("2 PPQN")); break;
    case Clock::SOURCE_EXTERNAL_PPQN_1: copyP(g_sub, sizeof(g_sub), F("1 PPQN")); break;
    case Clock::SOURCE_EXTERNAL_MIDI: copyP(g_sub, sizeof(g_sub), F("MIDI")); break;
    default: break;
    }
    break;
  case PARAM_MAIN_PULSE:
    copyP(g_main, sizeof(g_main), F("OUT"));
    switch (app.selected_pulse) {
    case Clock::PULSE_NONE: copyP(g_sub, sizeof(g_sub), F("PULSE OFF")); break;
    case Clock::PULSE_PPQN_24: copyP(g_sub, sizeof(g_sub), F("24 PPQN PULSE")); break;
    case Clock::PULSE_PPQN_4: copyP(g_sub, sizeof(g_sub), F("4 PPQN PULSE")); break;
    case Clock::PULSE_PPQN_1: copyP(g_sub, sizeof(g_sub), F("1 PPQN PULSE")); break;
    default: break;
    }
    break;
  case PARAM_MAIN_ENCODER_DIR: {
    copyP(g_main, sizeof(g_main), F("DIR"));
    bool reversed = app.editing_param ? (app.selected_sub_param == 1)
                                      : app.encoder_reversed;
    copyP(g_sub, sizeof(g_sub), reversed ? F("REVERSED") : F("DEFAULT"));
    break;
  }
  case PARAM_MAIN_ROTATE_DISP: {
    copyP(g_main, sizeof(g_main), F("ROT"));
    bool rotated = app.editing_param ? (app.selected_sub_param == 1)
                                     : app.rotate_display;
    copyP(g_sub, sizeof(g_sub), rotated ? F("ROTATED") : F("DEFAULT"));
    break;
  }
  case PARAM_MAIN_SAVE_DATA:
  case PARAM_MAIN_LOAD_DATA:
    if (app.selected_sub_param == StateManager::MAX_SAVE_SLOTS) {
      copyP(g_main, sizeof(g_main), F("x"));
      copyP(g_sub, sizeof(g_sub), F("BACK TO MAIN"));
    } else {
      if (app.selected_sub_param == app.selected_save_slot) {
        solidTick();
      }
      displaySaveSlot(g_main, app.selected_sub_param);
      copyP(g_sub, sizeof(g_sub),
            (app.selected_param == PARAM_MAIN_SAVE_DATA) ? F("SAVE TO SLOT")
                                                         : F("LOAD FROM SLOT"));
    }
    break;
  case PARAM_MAIN_RESET_STATE:
    if (app.selected_sub_param == 0) {
      copyP(g_main, sizeof(g_main), F("RST"));
      copyP(g_sub, sizeof(g_sub), F("RESET ALL"));
    } else {
      copyP(g_main, sizeof(g_main), F("x"));
      copyP(g_sub, sizeof(g_sub), F("BACK TO MAIN"));
    }
    break;
  case PARAM_MAIN_FACTORY_RESET:
    if (app.selected_sub_param == 0) {
      copyP(g_main, sizeof(g_main), F("DEL"));
      copyP(g_sub, sizeof(g_sub), F("FACTORY RESET"));
    } else {
      copyP(g_main, sizeof(g_main), F("x"));
      copyP(g_sub, sizeof(g_sub), F("BACK TO MAIN"));
    }
    break;
  }

  if (show_cv_meter) {
    drawCvMeter(cv_meter_value, 2, 18, 60, 12);
  } else {
    drawCenteredText(g_main, MAIN_TEXT_Y, LARGE_FONT);
  }
  drawCenteredText(g_sub, SUB_TEXT_Y, TEXT_FONT);

  const __FlashStringHelper *menu_items[PARAM_MAIN_LAST] = {
      F("TEMPO"),       F("RUN"),         F("RESTART"),
      F("SOURCE"),      F("PULSE OUT"),   F("ENCODER DIR"),
      F("ROTATE DISP"), F("SAVE"),        F("LOAD"),
      F("RESET"),
      F("CV1 CAL -5V"), F("CV1 CAL 0V"),  F("CV1 CAL +5V"),
      F("CV2 CAL -5V"), F("CV2 CAL 0V"),  F("CV2 CAL +5V"),
      F("ERASE")};
  drawMenuItems(menu_items, PARAM_MAIN_LAST);
}

// Human-friendly label for a CV routing target.
const __FlashStringHelper *cvTargetLabel(CvTarget t) {
  switch (t) {
  case CV_NONE: return F("NONE");
  default: return Channel::paramLabel(t - 1);
  }
}

// Per-channel page: clock mod, the six gate params, then the two CV targets.
// Draw the channel's euclidean pattern along the top: 3x3 px per step, filled
// box for a hit, frame for a rest, centered on the step count.
void drawChannelPattern(const Channel &ch) {
  const uint8_t step_box_size = 4;
  uint8_t steps = ch.patternSteps();
  int x0 = (SCREEN_WIDTH - steps * step_box_size) / 2;
  gravity.display.setDrawColor(1);
  for (uint8_t i = 0; i < steps; ++i) {
    int x = x0 + i * step_box_size;
    if (ch.patternHit(i))
      gravity.display.drawBox(x, 0, step_box_size, step_box_size);
    else
      gravity.display.drawFrame(x, 0, step_box_size, step_box_size);
  }
  gravity.display.setDrawColor(2);
}

void DisplayChannelPage() {
  auto &ch = GetSelectedChannel();

  gravity.display.setFontMode(1);
  gravity.display.setDrawColor(2);

  drawChannelPattern(ch);

  g_main[0] = '\0';
  g_sub[0] = '\0';

  // When editing show the base value; otherwise the CV-modulated value.
  bool withCvMod = !app.editing_param;
  const uint8_t param = app.selected_param;

  if (param == CP_CLOCK_MOD) {
    int mod_value = ch.getClockMod(withCvMod);
    if (mod_value > 1) {
      g_main[0] = '/';
      itoa(mod_value, g_main + 1, 10);
      copyP(g_sub, sizeof(g_sub), F("DIVIDE"));
    } else {
      g_main[0] = 'x';
      itoa(abs(mod_value), g_main + 1, 10);
      copyP(g_sub, sizeof(g_sub), F("MULTIPLY"));
    }
  } else if (pageParamIsGate(param)) {
    itoa(ch.paramValue(param, withCvMod), g_main, 10);
    copyP(g_sub, sizeof(g_sub), Channel::paramLabel(param));
  } else if (param == CP_CHOKE) {
    uint8_t src = ch.getChoke();
    if (src == 0)
      copyP(g_main, sizeof(g_main), F("OFF"));
    else
      itoa(src, g_main, 10);
    copyP(g_sub, sizeof(g_sub), F("CHOKE BY"));
  } else {
    bool is_cv1 = (param == CP_CV1);
    copyP(g_main, sizeof(g_main), is_cv1 ? F("CV1") : F("CV2"));
    copyP(g_sub, sizeof(g_sub),
          cvTargetLabel(is_cv1 ? ch.getCv1Target() : ch.getCv2Target()));
  }

  drawCenteredText(g_main, MAIN_TEXT_Y, LARGE_FONT);
  drawCenteredText(g_sub, SUB_TEXT_Y, TEXT_FONT);

  // Labels come from Channel::paramLabel (single source), indexed by ChannelPageParam.
  const __FlashStringHelper *menu_items_channel[CHANNEL_PAGE_PARAM_COUNT];
  for (uint8_t i = 0; i < CHANNEL_PAGE_PARAM_COUNT; ++i)
    menu_items_channel[i] = Channel::paramLabel(i);
  drawMenuItems(menu_items_channel, CHANNEL_PAGE_PARAM_COUNT);
}

void DisplaySelectedChannel() {
  int boxY = CHANNEL_BOXES_Y;
  int boxWidth = CHANNEL_BOX_WIDTH;
  int boxHeight = CHANNEL_BOX_HEIGHT;
  int textOffset = 7; // Half of font width

  gravity.display.drawHLine(1, boxY, SCREEN_WIDTH - 2);
  gravity.display.drawVLine(SCREEN_WIDTH - 2, boxY, boxHeight);

  for (uint8_t i = 0; i < Gravity::OUTPUT_COUNT + 1; ++i) {
    gravity.display.setDrawColor(1);
    (app.selected_channel == i)
        ? gravity.display.drawBox(i * boxWidth, boxY, boxWidth, boxHeight)
        : gravity.display.drawVLine(i * boxWidth, boxY, boxHeight);

    gravity.display.setDrawColor(2);
    if (i == 0) {
      gravity.display.setBitmapMode(1);
      auto icon = gravity.clock.IsPaused() ? pause_icon : play_icon;
      gravity.display.drawXBMP(2, boxY, play_icon_width, play_icon_height, icon);
    } else {
      gravity.display.setFont(TEXT_FONT);
      gravity.display.setCursor((i * boxWidth) + textOffset, SCREEN_HEIGHT - 3);
      if (app.channel[i - 1].isMuted()) {
        gravity.display.print("M");
      } else {
        gravity.display.print(i);
      }
    }
  }
}

void UpdateDisplay() {
  app.refresh_screen = false;
  gravity.display.firstPage();
  do {
    if (app.selected_channel == 0) {
      DisplayMainPage();
    } else {
      DisplayChannelPage();
    }
    DisplaySelectedChannel();
  } while (gravity.display.nextPage());
}

void Bootsplash() {
  gravity.display.firstPage();
  do {
    int textWidth;
    gravity.display.setFont(TEXT_FONT);

    textWidth = gravity.display.getStrWidth(StateManager::SKETCH_NAME);
    gravity.display.drawStr(4 + (textWidth / 2), 22, StateManager::SKETCH_NAME);

    textWidth = gravity.display.getStrWidth(StateManager::SEMANTIC_VERSION);
    gravity.display.drawStr(16 + (textWidth / 2), 32,
                            StateManager::SEMANTIC_VERSION);

    copyP(g_main, sizeof(g_main), F("LOADING...."));
    textWidth = gravity.display.getStrWidth(g_main);
    gravity.display.drawStr(26 + (textWidth / 2), 44, g_main);
  } while (gravity.display.nextPage());
}

#endif // DISPLAY_H
