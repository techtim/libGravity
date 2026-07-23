/**
 * @file display.h
 * @author Adam Wonak (https://github.com/awonak/)
 * @brief Alt firmware version of Gravity by Sitka Instruments.
 * @version 2.0.2
 * @date 2025-07-04
 *
 * @copyright MIT - (c) 2025 - Adam Wonak - adam.wonak@gmail.com
 *
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
constexpr uint8_t MAIN_TEXT_Y = 26;
constexpr uint8_t SUB_TEXT_Y = 40;
constexpr uint8_t VISIBLE_MENU_ITEMS = 3;
constexpr uint8_t MENU_ITEM_HEIGHT = 14;
constexpr uint8_t MENU_BOX_PADDING = 4;
constexpr uint8_t MENU_BOX_WIDTH = 64;
constexpr uint8_t CHANNEL_BOXES_Y = 50;
constexpr uint8_t CHANNEL_BOX_WIDTH = 18;
constexpr uint8_t CHANNEL_BOX_HEIGHT = 14;

// Menu items for editing global parameters.
enum ParamsMainPage : uint8_t {
  PARAM_MAIN_TEMPO,
  PARAM_MAIN_SOURCE,
  PARAM_MAIN_RUN,
  PARAM_MAIN_RESET,
  PARAM_MAIN_PULSE,
  PARAM_MAIN_ENCODER_DIR,
  PARAM_MAIN_ROTATE_DISP,
  PARAM_MAIN_SAVE_DATA,
  PARAM_MAIN_LOAD_DATA,
  PARAM_MAIN_RESET_STATE,
  PARAM_MAIN_FACTORY_RESET,
  PARAM_MAIN_LAST,
};

// Menu items for editing channel parameters.
enum ParamsChannelPage : uint8_t {
  PARAM_CH_MOD,
  PARAM_CH_PROB,
  PARAM_CH_DUTY,
  PARAM_CH_OFFSET,
  PARAM_CH_SWING,
  PARAM_CH_CV1_DEST,
  PARAM_CH_CV2_DEST,
  PARAM_CH_LAST,
};

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
  const int tickSize = 3;
  const int mainWidth = SCREEN_WIDTH / 2;
  const int mainHeight = 49;
  gravity.display.drawLine(0, 0, tickSize, 0);
  gravity.display.drawLine(0, 0, 0, tickSize);
  gravity.display.drawLine(mainWidth, 0, mainWidth - tickSize, 0);
  gravity.display.drawLine(mainWidth, 0, mainWidth, tickSize);
  gravity.display.drawLine(mainWidth, mainHeight, mainWidth,
                           mainHeight - tickSize);
  gravity.display.drawLine(mainWidth, mainHeight, mainWidth - tickSize,
                           mainHeight);
  gravity.display.drawLine(0, mainHeight, tickSize, mainHeight);
  gravity.display.drawLine(0, mainHeight, 0, mainHeight - tickSize);
  gravity.display.setDrawColor(2);
}

void drawMenuItems(String menu_items[], int menu_size) {
  // Draw menu items
  gravity.display.setFont(TEXT_FONT);

  // Draw selected menu item box
  int selectedBoxY = 0;
  if (menu_size >= VISIBLE_MENU_ITEMS && app.selected_param == menu_size - 1) {
    selectedBoxY = MENU_ITEM_HEIGHT * min(2, app.selected_param);
  } else if (app.selected_param > 0) {
    selectedBoxY = MENU_ITEM_HEIGHT;
  }

  int boxX = MENU_BOX_WIDTH + 1;
  int boxY = selectedBoxY + 2;
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

  for (int i = 0; i < min(menu_size, VISIBLE_MENU_ITEMS); ++i) {
    int idx = start_index + i;
    drawRightAlignedText(menu_items[idx].c_str(),
                         MENU_ITEM_HEIGHT * (i + 1) - 1);
  }
}

// Visual indicators for main section of screen.
inline void solidTick() { gravity.display.drawBox(56, 4, 4, 4); }
inline void hollowTick() { gravity.display.drawBox(56, 4, 4, 4); }

// Display an indicator when swing percentage matches a musical note.
void swingDivisionMark() {
  auto &ch = GetSelectedChannel();
  switch (ch.getSwing()) {
  case 58: // 1/32nd
  case 66: // 1/16th
  case 75: // 1/8th
    solidTick();
    break;
  case 54: // 1/32nd tripplet
  case 62: // 1/16th tripplet
  case 71: // 1/8th tripplet
    hollowTick();
    break;
  }
}

// Human friendly display value for save slot.
String displaySaveSlot(int slot) {
  if (slot >= 0 && slot < StateManager::MAX_SAVE_SLOTS / 2) {
    return String("A") + String(slot + 1);
  } else if (slot >= StateManager::MAX_SAVE_SLOTS / 2 &&
             slot <= StateManager::MAX_SAVE_SLOTS) {
    return String("B") + String(slot - (StateManager::MAX_SAVE_SLOTS / 2) + 1);
  }
}

// Main display functions

void DisplayMainPage() {
  gravity.display.setFontMode(1);
  gravity.display.setDrawColor(2);
  gravity.display.setFont(TEXT_FONT);

  // Display selected editable value
  String mainText;
  String subText;

  switch (app.selected_param) {
  case PARAM_MAIN_TEMPO:
    // Serial MIDI is too unstable to display bpm in real time.
    if (app.selected_source == Clock::SOURCE_EXTERNAL_MIDI) {
      mainText = F("EXT");
    } else {
      mainText = String(gravity.clock.Tempo());
    }
    subText = F("BPM");
    break;
  case PARAM_MAIN_SOURCE:
    mainText = F("EXT");
    switch (app.selected_source) {
    case Clock::SOURCE_INTERNAL:
      mainText = F("INT");
      subText = F("CLOCK");
      break;
    case Clock::SOURCE_EXTERNAL_PPQN_24:
      subText = F("24 PPQN");
      break;
    case Clock::SOURCE_EXTERNAL_PPQN_4:
      subText = F("4 PPQN");
      break;
    case Clock::SOURCE_EXTERNAL_PPQN_2:
      subText = F("2 PPQN");
      break;
    case Clock::SOURCE_EXTERNAL_PPQN_1:
      subText = F("1 PPQN");
      break;
    case Clock::SOURCE_EXTERNAL_MIDI:
      subText = F("MIDI");
      break;
    }
    break;
  case PARAM_MAIN_RUN:
    mainText = F("RUN");
    switch (app.cv_run) {
    case 0:
      subText = F("NONE");
      break;
    case 1:
      subText = F("CV 1");
      break;
    case 2:
      subText = F("CV 2");
      break;
    }
    break;
  case PARAM_MAIN_RESET:
    mainText = F("RST");
    switch (app.cv_reset) {
    case 0:
      subText = F("NONE");
      break;
    case 1:
      subText = F("CV 1");
      break;
    case 2:
      subText = F("CV 2");
      break;
    }
    break;
  case PARAM_MAIN_PULSE:
    mainText = F("OUT");
    switch (app.selected_pulse) {
    case Clock::PULSE_NONE:
      subText = F("PULSE OFF");
      break;
    case Clock::PULSE_PPQN_24:
      subText = F("24 PPQN PULSE");
      break;
    case Clock::PULSE_PPQN_4:
      subText = F("4 PPQN PULSE");
      break;
    case Clock::PULSE_PPQN_1:
      subText = F("1 PPQN PULSE");
      break;
    }
    break;
  case PARAM_MAIN_ENCODER_DIR:
    mainText = F("DIR");
    subText = app.selected_sub_param == 0 ? F("DEFAULT") : F("REVERSED");
    break;
  case PARAM_MAIN_ROTATE_DISP:
    mainText = F("DISP");
    subText = app.selected_sub_param == 0 ? F("DEFAULT") : F("ROTATED");
    break;
  case PARAM_MAIN_SAVE_DATA:
  case PARAM_MAIN_LOAD_DATA:
    if (app.selected_sub_param == StateManager::MAX_SAVE_SLOTS) {
      mainText = F("x");
      subText = F("BACK TO MAIN");
    } else {
      // Indicate currently active slot.
      if (app.selected_sub_param == app.selected_save_slot) {
        solidTick();
      }
      mainText = displaySaveSlot(app.selected_sub_param);
      subText = (app.selected_param == PARAM_MAIN_SAVE_DATA)
                    ? F("SAVE TO SLOT")
                    : F("LOAD FROM SLOT");
    }
    break;
  case PARAM_MAIN_RESET_STATE:
    if (app.selected_sub_param == 0) {
      mainText = F("RST");
      subText = F("RESET ALL");
    } else {
      mainText = F("x");
      subText = F("BACK TO MAIN");
    }
    break;
  case PARAM_MAIN_FACTORY_RESET:
    if (app.selected_sub_param == 0) {
      mainText = F("DEL");
      subText = F("FACTORY RESET");
    } else {
      mainText = F("x");
      subText = F("BACK TO MAIN");
    }
    break;
  }

  drawCenteredText(mainText.c_str(), MAIN_TEXT_Y, LARGE_FONT);
  drawCenteredText(subText.c_str(), SUB_TEXT_Y, TEXT_FONT);

  // Draw Main Page menu items
  String menu_items[PARAM_MAIN_LAST] = {
      F("TEMPO"),     F("RUN"),         F("RST"),         F("SOURCE"),
      F("PULSE OUT"), F("ENCODER DIR"), F("ROTATE DISP"), F("SAVE"),
      F("LOAD"),      F("RESET"),       F("ERASE")};
  drawMenuItems(menu_items, PARAM_MAIN_LAST);
}

void DisplayChannelPage() {
  auto &ch = GetSelectedChannel();

  gravity.display.setFontMode(1);
  gravity.display.setDrawColor(2);

  // Display selected editable value
  String mainText;
  String subText;

  // When editing a param, just show the base value. When not editing show
  // the value with cv mod.
  bool withCvMod = !app.editing_param;

  switch (app.selected_param) {
  case PARAM_CH_MOD: {
    int mod_value = ch.getClockMod(withCvMod);
    if (mod_value > 1) {
      mainText = F("/");
      mainText += String(mod_value);
      subText = F("DIVIDE");
    } else {
      mainText = F("x");
      mainText += String(abs(mod_value));
      subText = F("MULTIPLY");
    }
    break;
  }
  case PARAM_CH_PROB:
    mainText = String(ch.getProbability(withCvMod)) + F("%");
    subText = F("HIT CHANCE");
    break;
  case PARAM_CH_DUTY:
    mainText = String(ch.getDutyCycle(withCvMod)) + F("%");
    subText = F("PULSE WIDTH");
    break;
  case PARAM_CH_OFFSET:
    mainText = String(ch.getOffset(withCvMod)) + F("%");
    subText = F("SHIFT HIT");
    break;
  case PARAM_CH_SWING:
    ch.getSwing() == 50 ? mainText = F("OFF")
                        : mainText = String(ch.getSwing(withCvMod)) + F("%");
    subText = "DOWN BEAT";
    swingDivisionMark();
    break;
  case PARAM_CH_CV1_DEST:
  case PARAM_CH_CV2_DEST: {
    mainText = (app.selected_param == PARAM_CH_CV1_DEST) ? F("CV1") : F("CV2");
    switch ((app.selected_param == PARAM_CH_CV1_DEST) ? ch.getCv1Dest()
                                                      : ch.getCv2Dest()) {
    case CV_DEST_NONE:
      subText = F("NONE");
      break;
    case CV_DEST_MOD:
      subText = F("CLOCK MOD");
      break;
    case CV_DEST_PROB:
      subText = F("PROBABILITY");
      break;
    case CV_DEST_DUTY:
      subText = F("DUTY CYCLE");
      break;
    case CV_DEST_OFFSET:
      subText = F("OFFSET");
      break;
    case CV_DEST_SWING:
      subText = F("SWING");
      break;
    }
    break;
  }
  }

  drawCenteredText(mainText.c_str(), MAIN_TEXT_Y, LARGE_FONT);
  drawCenteredText(subText.c_str(), SUB_TEXT_Y, TEXT_FONT);

  // Draw Channel Page menu items
  String menu_items[PARAM_CH_LAST] = {
      F("MOD"),   F("PROBABILITY"), F("DUTY"),   F("OFFSET"),
      F("SWING"), F("CV1 MOD"),     F("CV2 MOD")};
  drawMenuItems(menu_items, PARAM_CH_LAST);
}

void DisplaySelectedChannel() {
  int boxX = CHANNEL_BOX_WIDTH;
  int boxY = CHANNEL_BOXES_Y;
  int boxWidth = CHANNEL_BOX_WIDTH;
  int boxHeight = CHANNEL_BOX_HEIGHT;
  int textOffset = 7; // Half of font width

  // Draw top and right side of frame.
  gravity.display.drawHLine(1, boxY, SCREEN_WIDTH - 2);
  gravity.display.drawVLine(SCREEN_WIDTH - 2, boxY, boxHeight);

  for (int i = 0; i < Gravity::OUTPUT_COUNT + 1; i++) {
    // Draw box frame or filled selected box.
    gravity.display.setDrawColor(1);
    (app.selected_channel == i)
        ? gravity.display.drawBox(i * boxWidth, boxY, boxWidth, boxHeight)
        : gravity.display.drawVLine(i * boxWidth, boxY, boxHeight);

    // Draw clock status icon or each channel number.
    gravity.display.setDrawColor(2);
    if (i == 0) {
      gravity.display.setBitmapMode(1);
      auto icon = gravity.clock.IsPaused() ? pause_icon : play_icon;
      gravity.display.drawXBMP(2, boxY, play_icon_width, play_icon_height,
                               icon);
    } else {
      gravity.display.setFont(TEXT_FONT);
      gravity.display.setCursor((i * boxWidth) + textOffset, SCREEN_HEIGHT - 3);
      gravity.display.print(i);
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
    // Global channel select UI.
    DisplaySelectedChannel();
  } while (gravity.display.nextPage());
}

void Bootsplash() {
  gravity.display.firstPage();
  do {
    int textWidth;
    String loadingText = F("LOADING....");
    gravity.display.setFont(TEXT_FONT);

    textWidth = gravity.display.getStrWidth(StateManager::SKETCH_NAME);
    gravity.display.drawStr(16 + (textWidth / 2), 20,
                            StateManager::SKETCH_NAME);

    textWidth = gravity.display.getStrWidth(StateManager::SEMANTIC_VERSION);
    gravity.display.drawStr(16 + (textWidth / 2), 32,
                            StateManager::SEMANTIC_VERSION);

    textWidth = gravity.display.getStrWidth(loadingText.c_str());
    gravity.display.drawStr(26 + (textWidth / 2), 44, loadingText.c_str());
  } while (gravity.display.nextPage());
}

#endif // DISPLAY_H
