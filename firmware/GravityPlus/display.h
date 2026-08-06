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

static char g_main[16];
static char g_sub[20];

// Append a single char to a C string (cheaper than strcat: no library symbol,
// no string-literal operand).
inline void appendChar(char *s, char c) {
  while (*s)
    s++;
  *s++ = c;
  *s = '\0';
}

// Global-page menu labels. Kept as an explicit PROGMEM pointer table rather
// than a switch: avr-gcc compiles a switch that returns distinct pointers into
// a table in .rodata, which on AVR is copied into RAM at startup.
const char L_TEMPO[] PROGMEM = "TEMPO";
const char L_RUN[] PROGMEM = "RUN";
const char L_RESTART[] PROGMEM = "RESTART";
const char L_SOURCE[] PROGMEM = "SOURCE";
const char L_PULSE[] PROGMEM = "PULSE OUT";
const char L_ENC_DIR[] PROGMEM = "ENCODER DIR";
const char L_ROT_DISP[] PROGMEM = "ROTATE DISP";
const char L_BTN_MODE[] PROGMEM = "BTN MODE";
const char L_SAVE[] PROGMEM = "SAVE";
const char L_LOAD[] PROGMEM = "LOAD";
const char L_RESET[] PROGMEM = "RESET";
const char L_CV1_LO[] PROGMEM = "CV1 CAL -5V";
const char L_CV1_ZERO[] PROGMEM = "CV1 CAL 0V";
const char L_CV1_HI[] PROGMEM = "CV1 CAL +5V";
const char L_CV2_LO[] PROGMEM = "CV2 CAL -5V";
const char L_CV2_ZERO[] PROGMEM = "CV2 CAL 0V";
const char L_CV2_HI[] PROGMEM = "CV2 CAL +5V";
const char L_ERASE[] PROGMEM = "ERASE";
const char *const MAIN_LABELS[PARAM_MAIN_LAST] PROGMEM = {
    L_TEMPO,    L_RUN,      L_RESTART,  L_SOURCE,   L_PULSE,    L_ENC_DIR,
    L_ROT_DISP, L_BTN_MODE, L_SAVE,     L_LOAD,     L_RESET,    L_CV1_LO,
    L_CV1_ZERO, L_CV1_HI,   L_CV2_LO,   L_CV2_ZERO, L_CV2_HI,   L_ERASE};

// Channel-page labels, indexed by ChannelPageParam. Same PROGMEM-table reason
// as MAIN_LABELS above; single source of truth for the channel-page strings.
const char C_CLOCK_MOD[] PROGMEM = "CLOCK MOD";
const char C_STEPS[] PROGMEM = "STEPS";
const char C_HITS[] PROGMEM = "HITS";
const char C_ROTATE[] PROGMEM = "ROTATE";
const char C_PROB[] PROGMEM = "PROBAB";
const char C_DUTY[] PROGMEM = "DUTY";
const char C_OFFSET[] PROGMEM = "OFFSET";
const char C_SWING[] PROGMEM = "SWING";
const char C_CHOKE[] PROGMEM = "CHOKE";
const char C_CV1A[] PROGMEM = "CV1-A";
const char C_CV1B[] PROGMEM = "CV1-B";
const char C_CV2A[] PROGMEM = "CV2-A";
const char C_CV2B[] PROGMEM = "CV2-B";
const char *const CHANNEL_LABELS[] PROGMEM = {
    C_CLOCK_MOD, C_STEPS, C_HITS,  C_ROTATE, C_PROB,  C_DUTY, C_OFFSET,
    C_SWING,     C_CHOKE, C_CV1A,  C_CV1B,   C_CV2A,  C_CV2B};

const __FlashStringHelper *channelParamLabel(uint8_t i) {
  return (const __FlashStringHelper *)pgm_read_word(&CHANNEL_LABELS[i]);
}

const __FlashStringHelper *mainParamLabel(uint8_t i) {
  return (const __FlashStringHelper *)pgm_read_word(&MAIN_LABELS[i]);
}

// Copy a flash (PROGMEM) string into a RAM buffer for u8g2 text functions.
inline void copyP(char *dst, size_t n, const __FlashStringHelper *f) {
  strncpy_P(dst, reinterpret_cast<PGM_P>(f), n - 1);
  dst[n - 1] = '\0';
}

// Helper function to draw centered text
void drawCenteredText(const char *text, uint8_t y, const uint8_t *font) {
  gravity.display.setFont(font);
  uint8_t textWidth = gravity.display.getStrWidth(text);
  gravity.display.drawStr(SCREEN_CENTER_X - (textWidth / 2), y, text);
}

// Helper function to draw right-aligned text
void drawRightAlignedText(const char *text, uint8_t y) {
  uint8_t textWidth = gravity.display.getStrWidth(text);
  uint8_t drawX = (SCREEN_WIDTH - textWidth) - MENU_BOX_PADDING;
  gravity.display.drawStr(drawX, y, text);
}

void drawMainSelection() {
  gravity.display.setDrawColor(1);
  const uint8_t offsetY = 6;
  const uint8_t tickSize = 3;
  const uint8_t mainWidth = SCREEN_WIDTH / 2;
  const uint8_t mainHeight = 46;
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

// Draw the channel's euclidean pattern along the top
void drawChannelPattern(const Channel &ch) {
  const uint8_t sz = 5; // px per step
  uint8_t steps = ch.patternSteps();
  if (steps != 1) {
    uint8_t x0 = (SCREEN_WIDTH - steps * sz) / 2; // centered on the step count
    const uint8_t w = steps * sz;
    gravity.display.setDrawColor(1);
    gravity.display.drawHLine(x0, sz - 1, w);
    for (uint8_t i = 0; i <= steps; ++i) {
      gravity.display.drawVLine(x0 + i * sz, 0, sz);
      if (i < steps && ch.patternHit(i))
        gravity.display.drawBox(x0 + i * sz, 0, sz, sz);
    }
  }
  const int mod_value = ch.getClockMod(true);
  g_sub[0] = mod_value > 1 ? '/' : 'x';
  itoa(abs(mod_value), g_sub + 1, 10);
  gravity.display.setFont(TEXT_FONT);
  gravity.display.drawStr(0, sz, g_sub);
}

// Labels are fetched one at a time through a lookup function: only the three
// visible rows are ever resolved, and no caller has to build a full pointer
// array on the stack.
typedef const __FlashStringHelper *(*MenuLabelFn)(uint8_t);

void drawMenuItems(MenuLabelFn menu_label, int menu_size) {
  gravity.display.setFont(TEXT_FONT);

  // Scroll window: the selection sits on the middle row, except at either end
  // of the list where the window is pinned. The highlight row then follows from
  // the selection's position inside that window.
  uint8_t start_index = 0;
  if (menu_size >= VISIBLE_MENU_ITEMS && app.selected_param == menu_size - 1) {
    start_index = menu_size - VISIBLE_MENU_ITEMS;
  } else if (app.selected_param > 0) {
    start_index = app.selected_param - 1;
  }
  uint8_t selectedBoxY = MENU_ITEM_HEIGHT * (app.selected_param - start_index);

  uint8_t boxX = MENU_BOX_WIDTH + 1;
  uint8_t boxY = MENU_ITEM_Y + selectedBoxY + 2;
  uint8_t boxWidth = MENU_BOX_WIDTH - 1;
  uint8_t boxHeight = MENU_ITEM_HEIGHT + 1;

  if (app.editing_param) {
    gravity.display.drawBox(boxX, boxY, boxWidth, boxHeight);
    drawMainSelection();
  } else {
    gravity.display.drawFrame(boxX, boxY, boxWidth, boxHeight);
  }

  // Draw the visible menu items
  for (uint8_t i = 0; i < min(menu_size, VISIBLE_MENU_ITEMS); ++i) {
    copyP(g_sub, sizeof(g_sub), menu_label(start_index + i));
    drawRightAlignedText(g_sub, MENU_ITEM_Y + MENU_ITEM_HEIGHT * (i + 1) - 1);
  }
}

// Visual indicator: mark the active save slot.
inline void solidTick() { gravity.display.drawBox(56, 4, 4, 4); }

// Center-zero horizontal bar meter for a bipolar CV reading (-512..+512). The
// fill grows right of centre for positive readings, left for negative.
void drawCvMeter(int value, uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
  const uint8_t half = w / 2;
  const uint8_t cx = x + half;
  gravity.display.setDrawColor(1);
  gravity.display.drawFrame(x, y, w, h);
  gravity.display.drawVLine(cx, y - 2, h + 4); // centre tick
  if (value >= 0) {
    uint8_t fill = constrain(map(value, 0, 512, 0, half), 0, half);
    if (fill > 0)
      gravity.display.drawBox(cx, y, fill, h);
  } else {
    uint8_t fill = constrain(map(-value, 0, 512, 0, half), 0, half);
    if (fill > 0)
      gravity.display.drawBox(cx - fill, y, fill, h);
  }
  gravity.display.setDrawColor(2);
}

// Human friendly display value for save slot, written into `out` (e.g. "A3").
void displaySaveSlot(char *out, int slot) {
  const uint8_t half = StateManager::MAX_SAVE_SLOTS / 2;
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
    itoa(app.cv_cal[app.selected_param - PARAM_MAIN_CV1_CAL_LO], g_main, 10); // cal value
    uint8_t cntr = 0;
    while (g_main[cntr])
      ++cntr;
    g_main[cntr] = ' ';
    g_main[cntr + 1] = '/';
    g_main[cntr + 2] = ' ';
    itoa(cv_meter_value, g_main + cntr + 3, 10);
    show_cv_meter = true; // tune against the live reading
    copyP(g_sub, sizeof(g_sub), mainParamLabel(app.selected_param));
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
    case Clock::PULSE_PPQN_24: copyP(g_sub, sizeof(g_sub), F("24 PPQN")); break;
    case Clock::PULSE_PPQN_4: copyP(g_sub, sizeof(g_sub), F("4 PPQN")); break;
    case Clock::PULSE_PPQN_1: copyP(g_sub, sizeof(g_sub), F("1 PPQN")); break;
    default: break;
    }
    break;
  // The three on/off preferences share one block: same DEFAULT / <alt> shape.
  case PARAM_MAIN_ENCODER_DIR:
  case PARAM_MAIN_ROTATE_DISP:
  case PARAM_MAIN_BTN_MODE: {
    const bool is_dir = app.selected_param == PARAM_MAIN_ENCODER_DIR;
    const bool is_rot = app.selected_param == PARAM_MAIN_ROTATE_DISP;
    copyP(g_main, sizeof(g_main),
          is_dir ? F("DIR") : is_rot ? F("ROT") : F("BTN"));
    const bool stored = is_dir   ? app.encoder_reversed
                        : is_rot ? app.rotate_display
                                 : app.invert_buttons;
    const bool on = app.editing_param ? (app.selected_sub_param == 1) : stored;
    copyP(g_sub, sizeof(g_sub), !on ? F("DEFAULT") : F("INVERTED"));
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
    gravity.display.drawStr(12, MAIN_TEXT_Y / 2, g_main);
    drawCvMeter(cv_meter_value, 2, 20, 60, 12);
  } else {
    drawCenteredText(g_main, MAIN_TEXT_Y, LARGE_FONT);
  }
  drawCenteredText(g_sub, SUB_TEXT_Y, TEXT_FONT);

  drawMenuItems(mainParamLabel, PARAM_MAIN_LAST);
}

// Human-friendly label for a CV routing target.
const __FlashStringHelper *cvTargetLabel(CvTarget t) {
  switch (t) {
  case CV_NONE: return F("NONE");
  default: return channelParamLabel(t - 1);
  }
}

void DisplayChannelPage() {
  gravity.display.setFontMode(1);

  auto &ch = GetSelectedChannel();
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
    // Percentage params get a '%' suffix.
    if (param == CP_PROB || param == CP_DUTY || param == CP_OFFSET ||
        param == CP_SWING)
      appendChar(g_main, '%');
    copyP(g_sub, sizeof(g_sub), channelParamLabel(param));
  } else if (param == CP_CHOKE) {
    uint8_t src = ch.getChoke();
    if (src == 0)
      copyP(g_main, sizeof(g_main), F("OFF"));
    else
      itoa(src, g_main, 10);
    copyP(g_sub, sizeof(g_sub), F("CHOKE BY"));
  } else {
    // CV mod slot (CV1-A/B, CV2-A/B): big value = amount, sub = destination.
    uint8_t slot = param - CP_CV1A;
    CvTarget dest = ch.getCvDest(slot);
    if (dest == CV_NONE) {
      copyP(g_main, sizeof(g_main), F("X"));
      copyP(g_sub, sizeof(g_sub), F("NONE"));
    } else {
      if (ch.getCvAmount(slot) < 0) {
        gravity.display.drawBox(0, 24, 4, 2); // '-' sign
      }
      itoa(abs(ch.getCvAmount(slot)), g_main, 10);
      appendChar(g_main, '%'); // amount is a depth percentage
      copyP(g_sub, sizeof(g_sub), cvTargetLabel(dest));
    }
  }

  drawCenteredText(g_main, MAIN_TEXT_Y, LARGE_FONT);
  drawCenteredText(g_sub, SUB_TEXT_Y, TEXT_FONT);

  // Labels come from channelParamLabel (single source), indexed by ChannelPageParam.
  drawMenuItems(channelParamLabel, CP_PARAM_COUNT);
}

void DisplaySelectedChannel() {
  uint8_t boxY = CHANNEL_BOXES_Y;
  uint8_t boxWidth = CHANNEL_BOX_WIDTH;
  uint8_t boxHeight = CHANNEL_BOX_HEIGHT;
  uint8_t textOffset = 7; // Half of font width

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
      gravity.display.drawXBMP(2, boxY, play_icon_width, play_icon_height, 
                              gravity.clock.IsPaused() ? pause_icon : play_icon );
    } else {
      // drawStr, not print(): Print::print(int) pulls in the number-formatting
      // path for what is only ever one character.
      gravity.display.setFont(TEXT_FONT);
      const char label[2] = {
          app.channel[i - 1].isMuted() ? 'M' : (char)('0' + i), '\0'};
      gravity.display.drawStr((i * boxWidth) + textOffset, SCREEN_HEIGHT - 3, label);
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
    uint8_t textWidth;

    gravity.display.setFont(LARGE_FONT);
    copyP(g_main, sizeof(g_main), F("AV"));
    gravity.display.drawStr(40, MAIN_TEXT_Y, g_main);
    copyP(g_main, sizeof(g_main), F("IT"));
    gravity.display.drawStr(74, MAIN_TEXT_Y, g_main);
    gravity.display.setFont(TEXT_FONT);

    copyP(g_sub, sizeof(g_sub), F("GR"));
    gravity.display.drawStr(28, MAIN_TEXT_Y - 8, g_sub);

    copyP(g_sub, sizeof(g_sub), F("Y PLUS"));
    gravity.display.drawStr(96, MAIN_TEXT_Y - 8, g_sub);

    textWidth = gravity.display.getStrWidth(StateManager::SEMANTIC_VERSION);
    gravity.display.drawStr(SCREEN_WIDTH / 2 - (textWidth / 2), 52,
                            StateManager::SEMANTIC_VERSION);

  } while (gravity.display.nextPage());
}

#endif // DISPLAY_H
