/*
  Copyright (C) 2024 Chase Taylor

  This file is part of chTRACKER at path ./src/screenUpdate.cxx

  chTRACKER is free software: you can redistribute it and/or modify it under
  the terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version.

  chTRACKER is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
  A PARTICULAR PURPOSE. See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along with
  chTRACKER. If not, see <https://www.gnu.org/licenses/>.

  Chase Taylor @ creset200@gmail.com
*/

/***************************************
 *                                     *
 *           INCLUDE SECTION           *
 *   ALL #include directives go here   *
 *                                     *
 ***************************************/

#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <array>
#include <climits>
#include <filesystem>
#include <fstream>
#include <iostream>
// #include <stdexcept>
#include <string>

#include "headers/log.hxx"
#include "log.hxx"
#include "main.h"

#include "channel.hxx"
#include "order.hxx"
#include "visual.h"

/**************************************
 *                                    *
 *            MACRO SECTION           *
 *   ALL #define DIRECTIVES GO HERE   *
 *                                    *
 **************************************/

#define TILE_SIZE 96
#define TILE_SIZE_F 96.0

#if defined(_WIN32)
#define PATH_SEPERATOR_S "\\"
#define PATH_SEPERATOR '\\'
#else
#define PATH_SEPERATOR_S "/"
#define PATH_SEPERATOR '/'
#endif

/*****************************
 *                           *
 *     FUNCTIONS SECTION     *
 *   ALL FUNCTIONS GO HERE   *
 *                           *
 *****************************/

/*************************
 * Hexadecimal functions *
 *************************/

char hex(char a) {
  a &= 15;
  if (a > 9)
    return 'A' + a - 10;
  else
    return '0' + a;
}

void hex2(unsigned char a, char &c1, char &c2) {
  if (a >> 4 > 9)
    c1 = 'A' + (a >> 4) - 10;
  else
    c1 = '0' + (a >> 4);

  if ((a & 15) > 9)
    c2 = 'A' + (a & 15) - 10;
  else
    c2 = '0' + (a & 15);
}

void hex4(unsigned short a, char *s) {
  if ((a >> 12 & 15) > 9)
    s[0] = 'A' + (a >> 12 & 15) - 10;
  else
    s[0] = '0' + (a >> 12 & 15);

  if ((a >> 8 & 15) > 9)
    s[1] = 'A' + (a >> 8 & 15) - 10;
  else
    s[1] = '0' + (a >> 8 & 15);

  if ((a >> 4 & 15) > 9)
    s[2] = 'A' + (a >> 4 & 15) - 10;
  else
    s[2] = '0' + (a >> 4 & 15);

  if ((a & 15) > 9)
    s[3] = 'A' + (a & 15) - 10;
  else
    s[3] = '0' + (a & 15);
}

/*********************
 * Typename function *
 *********************/

#ifndef IN_CHTRACKER_CONTEXT
char *getTypeName(audioChannelType type, bool short_name);
#endif

/***********************
 * Seperator functions *
 ***********************/

void barrier(SDL_Renderer *r, unsigned int y, int windowWidth) {
  SDL_SetRenderDrawColor(r, 63, 127, 255, 255);
  SDL_Rect borderRectangle = {0, static_cast<int>(y), windowWidth, 16};
  SDL_RenderFillRect(r, &borderRectangle);
}

void barrierVertical(SDL_Renderer *r, unsigned int x, int windowHeight) {
  SDL_SetRenderDrawColor(r, 63, 127, 255, 255);
  SDL_Rect borderRectangle = {static_cast<int>(x), 16, 16, windowHeight - 16};
  SDL_RenderFillRect(r, &borderRectangle);
}

/*********************************************************************
 *                                                                   *
 *                         MAIN GUI FUNCTION                         *
 *   AKA THE GIANT FUNCTION THAT DOES ALL OF THE HEAVY GUI LIFTING   *
 *                                                                   *
 *********************************************************************/

namespace guiFunc {
void background(SDL_Renderer *renderer,
                const unsigned int windowHorizontalTileCount,
                const unsigned int windowVerticalTileCount, const long millis) {
  for (unsigned int i = 0;
       i < windowHorizontalTileCount * windowVerticalTileCount; i++) {
    int horizontalTile = i % windowHorizontalTileCount;
    int verticalTile =
        (i / windowHorizontalTileCount) % windowVerticalTileCount;
    unsigned char tileBrightness;
    tileBrightness =
        (static_cast<unsigned char>((horizontalTile + 4) * (verticalTile + 7) *
                                    (millis / 2000.0)) &
         31) +
        16;
    text_drawBigChar(renderer, indexes_charToIdx('\x1b'), 12,
                     horizontalTile * TILE_SIZE, verticalTile * TILE_SIZE,
                     SDL_Color{static_cast<Uint8>(tileBrightness / 4),
                               static_cast<Uint8>(tileBrightness / 2),
                               tileBrightness, 255},
                     0);
  }
}

void titleScreen(SDL_Renderer *renderer, const int windowWidth,
                 const int windowHeight, const long millis,
                 const unsigned int fontTileCountW) {
  int i = 0;
  const char *str = "chTRACKER";
  while (str[i] != 0) {
    Uint8 r = i & 1 ? 255 : 128;
    Uint8 g = i & 2 ? 255 : 128;
    Uint8 b = i & 4 ? 255 : 128;
    text_drawBigChar(renderer, indexes_charToIdx(str[i]), 4,
                     windowWidth / 2 - 144 + (i * 32),
                     static_cast<int>(windowHeight / 3.0 +
                                      SDL_sin(i / 3.0 + (millis / 500.0)) * 16),
                     {r, g, b, 255}, 0);
    i++;
  }
  text_drawText(renderer, "Press Z", 3, windowWidth / 2 - 96,
                windowHeight * 2 / 3, visual_whiteText, 0, 10);
  {
    std::string versionString("Version ");
    char str[4];
    visual_numberToString(str, global_majorVersion);
    versionString += str;
    versionString += ".";
    visual_numberToString(str, global_minorVersion);
    versionString += str;
    versionString += ".";
    visual_numberToString(str, global_patchVersion);
    versionString += str;
    if (global_prereleaseVersion > 0) {
      char c[2];
      c[1] = 0;
      c[0] = 'A' + global_prereleaseVersion - 1;
      versionString += '.';
      versionString += c;
    }
    text_drawText(renderer, versionString.c_str(), 2, 0, windowHeight - 16,
                  visual_whiteText, 0, fontTileCountW);
    text_drawText(
        renderer,
        "NO WARRANTY for this program, to the extent permitted by law. In no "
        "event will the copyright holders be held liable for damages arising "
        "from this program. See the GNU General Public License.\nCopyright "
        "\xcc 2024 Chase Taylor.",
        1, 0, 0, visual_whiteText, 0, fontTileCountW * 2);
  }
}

void topBar(SDL_Renderer *renderer, const int windowWidth,
            const int windowHeight,
            const std::array<Sint16, WAVEFORM_SAMPLE_COUNT> &waveformDisplay,
            const bool isAudioPlaying, const bool hasUnsavedChanges,
            const GlobalMenus currentMenu) {
  if (isAudioPlaying) {
    unsigned int lastY = 0;
    unsigned int lastX = 0;
    for (size_t i = 0; i < WAVEFORM_SAMPLE_COUNT; i++) {
      unsigned int y;
      {
        float point = static_cast<float>(waveformDisplay.at(i));
        y = static_cast<unsigned int>(((point / (INT16_MAX + 1.0)) + 1.0) *
                                      (windowHeight - 16.0) / 2.0) +
            16;
      }
      unsigned int x = static_cast<unsigned int>(
          static_cast<float>(i) /
          static_cast<float>(WAVEFORM_SAMPLE_COUNT - 1) *
          static_cast<float>(windowWidth));
      line_drawLine(renderer, lastX, lastY, x, y,
                    {.r = 64, .g = 128, .b = 255, .a = 255});
      lastY = y;
      lastX = x;
    }
  }

  short xOffset = 0;
  text_drawText(renderer, "Help!", 1, xOffset, 0, visual_whiteText,
                currentMenu == GlobalMenus::help_menu, windowWidth / 8);
  xOffset += 6 * 8;
  text_drawText(renderer, "Order", 1, xOffset, 0, visual_whiteText,
                currentMenu == GlobalMenus::order_menu, windowWidth / 8);
  xOffset += 6 * 8;
  text_drawText(renderer, "Pat.", 1, xOffset, 0, visual_whiteText,
                currentMenu == GlobalMenus::pattern_menu, windowWidth / 8);
  xOffset += 5 * 8;
  text_drawText(renderer, "Inst.", 1, xOffset, 0, visual_whiteText,
                currentMenu == GlobalMenus::instrument_menu, windowWidth / 8);
  xOffset += 6 * 8;
  text_drawText(renderer, "OrdMan.", 1, xOffset, 0, visual_whiteText,
                currentMenu == GlobalMenus::order_management_menu,
                windowWidth / 8);
  xOffset += 8 * 8;
  text_drawText(renderer, "Options", 1, xOffset, 0, visual_whiteText,
                currentMenu == GlobalMenus::options_menu, windowWidth / 8);
  xOffset += 8 * 8;
  text_drawText(renderer, "File", 1, xOffset, 0, visual_whiteText,
                currentMenu == GlobalMenus::file_menu, windowWidth / 8);
  xOffset += 5 * 8;
  text_drawText(renderer, "Log", 1, xOffset, 0, visual_whiteText,
                currentMenu == GlobalMenus::log_menu, windowWidth / 8);
  xOffset += 4 * 8;
  if (hasUnsavedChanges)
    text_drawText(renderer, "Unsaved changes", 1, xOffset, 0,
                  currentMenu == GlobalMenus::quit_confirmation_menu
                      ? visual_redText
                      : visual_yellowText,
                  currentMenu == GlobalMenus::file_menu ||
                      currentMenu == GlobalMenus::quit_confirmation_menu,
                  windowWidth / 8);
  SDL_SetRenderDrawColor(renderer, 63, 127, 255, 255);
  SDL_Rect borderRectangle = {0, 8, windowWidth, 8};
  SDL_RenderFillRect(renderer, &borderRectangle);
}
} // namespace guiFunc

namespace guiMenus {
void help(SDL_Renderer *renderer, const std::filesystem::path &docPath,
          const CursorPos &cursorPosition, const int windowHeight) {
  std::ifstream helpFile(docPath / "help.txt", std::ios::in);
#ifdef _POSIX
  static bool usingBackupHelpFile = false;
  if (!helpFile.is_open()) {
    if (!usingBackupHelpFile) {
      cmd::log::debug("Using fallback documentation directory");
      usingBackupHelpFile = true;
    }
    helpFile.open("/usr/share/doc/chtracker/help.txt");
  } else
    usingBackupHelpFile = false;
#endif
  static bool couldntOpenHelp = false;
  if (!helpFile.is_open()) {
    if (!couldntOpenHelp) {
      cmd::log::error("Couldn't open help");
      couldntOpenHelp = true;
    }
    text_drawText(renderer, "Couldn't open help", 2, 0, 16, visual_redText, 1,
                  19);
    return;
  } else
    couldntOpenHelp = false;
  helpFile.seekg(0);
  if (helpFile.fail()) {
    text_drawText(renderer, "Couldn't read help", 2, 0, 16, visual_redText, 1,
                  19);
    return;
  }
  unsigned int undrawnLines = cursorPosition.y;
  unsigned int drawnRow = 0;
  std::array<char, 80> buf;
  while (!helpFile.eof() && !helpFile.fail()) {
    for (unsigned short i = 0; i < 80; i++) {
      buf[i] = 0;
    }
    helpFile.getline(buf.data(), 80);
    if (undrawnLines > 0) {
      undrawnLines--;
      continue;
    }
    int y = drawnRow * 16 + 16;
    if (y >= windowHeight)
      break;
    for (unsigned short i = 0; i < 80; i++) {
      if (buf[i] == 0)
        break;
      text_drawBigChar(renderer, indexes_charToIdx(buf[i]), 2, i * 16, y,
                       visual_whiteText, 0);
    }
    drawnRow++;
  }
}

void order(SDL_Renderer *renderer, const CursorPos &cursorPosition,
           const int windowWidth, const int windowHeight,
           const unsigned int fontTileCountW, const unsigned int fontTileCountH,
           orderIndexStorage &indexes) {
  text_drawText(renderer, "Order rows: ", 2, 0, 16, visual_whiteText, 0,
                fontTileCountW);
  std::array<char, 4> symbols;
  visual_numberToString(symbols.data(), indexes.rowCount());
  text_drawText(renderer, symbols.data(), 2, 13 * 16, 16, visual_whiteText, 0,
                fontTileCountW);
  text_drawText(renderer, "Z to add row", 2, 0, 32, visual_whiteText, 0,
                fontTileCountW);
  text_drawText(renderer, "X to remove selected row", 2, 0, 48,
                visual_whiteText, 0, fontTileCountW);
  text_drawText(renderer, "W to increment selected order", 2, 0, 64,
                visual_whiteText, 0, fontTileCountW);
  text_drawText(renderer, "S to decrement selected order", 2, 0, 80,
                visual_whiteText, 0, fontTileCountW);
  unsigned short startingRow = static_cast<unsigned short>(
      std::max(0, static_cast<int>(cursorPosition.y) -
                      static_cast<int>(fontTileCountH / 2)));
  unsigned short currentRow = 0;
  for (unsigned short i = startingRow; i < indexes.rowCount(); i++) {
    unsigned short y = 16 * (static_cast<unsigned short>(currentRow) + 8);
    if (y >= windowHeight)
      break;
    hex4(i, symbols.data());
    for (unsigned char hexNumberIndex = 0; hexNumberIndex < 4; hexNumberIndex++)
      text_drawBigChar(renderer, indexes_charToIdx(symbols[hexNumberIndex]), 2,
                       hexNumberIndex * 16, y, visual_whiteText,
                       i == cursorPosition.y);
    orderIndexRow *row = indexes.at(i);
    unsigned char startingCollumn = static_cast<unsigned char>(
        std::max(0, static_cast<short>(cursorPosition.x) -
                        (static_cast<short>(fontTileCountW) - 4) / 6));
    unsigned char currentCollumn = 0;
    for (unsigned char j = startingCollumn; j < row->instCount(); j++) {

      unsigned short x =
          16 * (static_cast<unsigned short>(currentCollumn * 3) + 5);
      if (x >= windowWidth)
        break;
      currentCollumn++;
      unsigned char orderIndex = row->at(j);

      hex2(orderIndex, symbols[0], symbols[1]);

      text_drawBigChar(renderer, indexes_charToIdx(symbols[0]), 2, x, y,
                       (i == cursorPosition.y && j == cursorPosition.x) ||
                               orderIndex != 0
                           ? visual_greenText
                           : SDL_Color{32, 64, 32, 255},
                       i == cursorPosition.y && j == cursorPosition.x);
      text_drawBigChar(renderer, indexes_charToIdx(symbols[1]), 2, x + 16, y,
                       (i == cursorPosition.y && j == cursorPosition.x) ||
                               orderIndex != 0
                           ? visual_greenText
                           : SDL_Color{32, 64, 32, 255},
                       i == cursorPosition.y && j == cursorPosition.x);

      if (currentRow == 0) {
        hex2(j, symbols[0], symbols[1]);

        text_drawBigChar(renderer, indexes_charToIdx(symbols[0]), 2, x, y - 16,
                         visual_whiteText, j == cursorPosition.x);
        text_drawBigChar(renderer, indexes_charToIdx(symbols[1]), 2, x + 16,
                         y - 16, visual_whiteText, j == cursorPosition.x);
      }
    }
    currentRow++;
  }
}

void pattern(SDL_Renderer *renderer, const int windowWidth,
             const int windowHeight, orderStorage &orders,
             const unsigned int fontTileCountW,
             const unsigned int fontTileCountH, orderIndexStorage &indexes,
             const bool isAudioPlaying, const unsigned short currentPattern,
             const unsigned short currentlyViewedOrder,
             instrumentStorage &instruments, const unsigned char currentRow,
             const CursorPos &cursorPosition, const char currentViewMode) {
  barrier(renderer, 48, windowWidth);
  if (orders.tableCount() == 0) {
    text_drawText(renderer, "Add instruments in the Inst. tab (F4)", 2, 0, 16,
                  visual_whiteText, 0, fontTileCountW);
    return;
  }
  text_drawText(renderer, "Order", 2, 0, 16, visual_whiteText, 0, 5);
  orderIndexRow *orderRow =
      indexes.at(isAudioPlaying ? currentPattern : currentlyViewedOrder);
  std::string letters = "1234";
  int cursorY = isAudioPlaying ? currentRow : cursorPosition.y;
  hex4(isAudioPlaying ? currentPattern : currentlyViewedOrder, letters.data());
  for (unsigned char hexNumberIndex = 0; hexNumberIndex < 4; hexNumberIndex++)
    text_drawBigChar(renderer, indexes_charToIdx(letters.at(hexNumberIndex)), 2,
                     (6 + hexNumberIndex) * 16, 16, visual_whiteText, 0);

  unsigned char selectedInstrument =
      cursorPosition.x /
      patternMenu_instrumentVariableCount[static_cast<size_t>(currentViewMode)];
  unsigned char selectedVariable =
      cursorPosition.x %
      patternMenu_instrumentVariableCount[static_cast<size_t>(currentViewMode)];

  short instrumentScreenCount =
      fontTileCountW /
      patternMenu_instrumentCollumnWidth[static_cast<size_t>(currentViewMode)];
  unsigned char firstInstrument =
      std::max(0, static_cast<short>(selectedInstrument) -
                      ((instrumentScreenCount - 1) / 2));

  unsigned char startingCollumn = static_cast<unsigned char>(
      std::max(0, static_cast<short>(selectedInstrument) -
                      static_cast<short>((fontTileCountH - 10) / 2)));
  unsigned short currentCollumn = 0;
  for (unsigned char collumnIndex = startingCollumn;
       collumnIndex < orderRow->instCount(); collumnIndex++) {
    unsigned char orderIndex = orderRow->at(collumnIndex);
    unsigned short x = currentCollumn * 48;
    if (x > windowWidth)
      break;
    hex2(orderIndex, letters.at(0), letters.at(1));
    text_drawBigChar(renderer, indexes_charToIdx(letters.at(0)), 2, x, 32,
                     visual_greenText, collumnIndex == selectedInstrument);
    text_drawBigChar(renderer, indexes_charToIdx(letters.at(1)), 2, x + 16, 32,
                     visual_greenText, collumnIndex == selectedInstrument);
    currentCollumn++;
  };
  currentCollumn = 0;
  for (unsigned char instrumentIndex = firstInstrument;
       instrumentIndex < orders.tableCount(); instrumentIndex++) {
    // if(16*(5+currentCollumn)>=windowWidth) break;
    class order *currentOrder =
        orders.at(instrumentIndex)->at(orderRow->at(instrumentIndex));

    unsigned short startingRow = static_cast<unsigned short>(
        std::max(0, cursorY - static_cast<int>((fontTileCountH - 3) / 2)));
    unsigned short currentRow = 0;
    for (unsigned short rowIndex = startingRow;
         rowIndex < currentOrder->rowCount(); rowIndex++) {
      unsigned short y = currentRow * 16 + 80;
      if (y >= windowHeight)
        break;
      if (currentCollumn >= fontTileCountW)
        break;
      unsigned short localCurrentCollumn = currentCollumn;
      bool rowSeleted =
          selectedInstrument == instrumentIndex && rowIndex == cursorY;

      if (currentViewMode >= 1 && currentCollumn == 0)
        text_drawBigChar(renderer, indexes_charToIdx('\x1c'), 2, 16 * 3, y,
                         visual_greyText, 0);

      if (currentCollumn == 0) {
        hex2(rowIndex, letters.at(0), letters.at(1));
        for (unsigned char hexNumberIndex = 0; hexNumberIndex < 2;
             hexNumberIndex++)
          text_drawBigChar(
              renderer, indexes_charToIdx(letters.at(hexNumberIndex)), 2,
              hexNumberIndex * 16, y, visual_greyText, cursorY == rowIndex);
      }
      if (currentRow == 0) {
        if (patternMenu_instrumentCollumnWidth[static_cast<size_t>(
                currentViewMode)] >= 13) {
          text_drawText(
              renderer,
              getTypeName(instruments.at(instrumentIndex)->get_type(), false),
              2, 16 * (7 + localCurrentCollumn), 64, visual_whiteText,
              instrumentIndex == selectedInstrument ||
                  (isAudioPlaying && cursorY == rowIndex),
              10);
        } else if (patternMenu_instrumentCollumnWidth[static_cast<size_t>(
                       currentViewMode)] >= 5) {
          text_drawText(
              renderer,
              getTypeName(instruments.at(instrumentIndex)->get_type(), true), 2,
              16 * (7 + localCurrentCollumn), 64, visual_whiteText,
              instrumentIndex == selectedInstrument ||
                  (isAudioPlaying && cursorY == rowIndex),
              2);
        }
        hex2(instrumentIndex, letters.at(0), letters.at(1));
        text_drawBigChar(renderer, indexes_charToIdx(letters.at(0)), 2,
                         16 * (4 + localCurrentCollumn), 64, visual_greenText,
                         instrumentIndex == selectedInstrument ||
                             (isAudioPlaying && cursorY == rowIndex));
        text_drawBigChar(renderer, indexes_charToIdx(letters.at(1)), 2,
                         16 * (5 + localCurrentCollumn), 64, visual_greenText,
                         instrumentIndex == selectedInstrument ||
                             (isAudioPlaying && cursorY == rowIndex));
      }

      row *r = currentOrder->at(rowIndex);
      switch (r->feature) {
      case rowFeature::note: {
        text_drawBigChar(renderer, indexes_charToIdx(hex(r->note - 'A')), 2,
                         16 * (4 + localCurrentCollumn), y, visual_blueText,
                         (rowSeleted && selectedVariable == 0) ||
                             (isAudioPlaying && cursorY == rowIndex));
        text_drawBigChar(renderer, indexes_charToIdx(hex(r->octave)), 2,
                         16 * (5 + localCurrentCollumn), y, visual_magentaText,
                         (rowSeleted && selectedVariable == 1) ||
                             (isAudioPlaying && cursorY == rowIndex));
        localCurrentCollumn += 3;
        if (currentViewMode >= 1) {
          hex2(r->volume, letters.at(0), letters.at(1));
          text_drawBigChar(renderer, indexes_charToIdx(letters.at(0)), 2,
                           16 * (4 + localCurrentCollumn), y, visual_greenText,
                           (rowSeleted && selectedVariable == 2) ||
                               (isAudioPlaying && cursorY == rowIndex));
          text_drawBigChar(renderer, indexes_charToIdx(letters.at(1)), 2,
                           16 * (5 + localCurrentCollumn), y, visual_greenText,
                           (rowSeleted && selectedVariable == 3) ||
                               (isAudioPlaying && cursorY == rowIndex));
          localCurrentCollumn += 3;
        }
        break;
      }
      case rowFeature::empty: {
        text_drawBigChar(renderer, indexes_charToIdx('.'), 2,
                         16 * (4 + localCurrentCollumn), y, visual_greyText,
                         (rowSeleted && selectedVariable == 0) ||
                             (isAudioPlaying && cursorY == rowIndex));
        text_drawBigChar(renderer, indexes_charToIdx('.'), 2,
                         16 * (5 + localCurrentCollumn), y, visual_greyText,
                         (rowSeleted && selectedVariable == 1) ||
                             (isAudioPlaying && cursorY == rowIndex));
        localCurrentCollumn += 3;
        if (currentViewMode >= 1) {
          text_drawBigChar(renderer, indexes_charToIdx('.'), 2,
                           16 * (4 + localCurrentCollumn), y, visual_greenText,
                           (rowSeleted && selectedVariable == 2) ||
                               (isAudioPlaying && cursorY == rowIndex));
          text_drawBigChar(renderer, indexes_charToIdx('.'), 2,
                           16 * (5 + localCurrentCollumn), y, visual_greenText,
                           (rowSeleted && selectedVariable == 3) ||
                               (isAudioPlaying && cursorY == rowIndex));
          localCurrentCollumn += 3;
        }
        break;
      }
      case rowFeature::note_cut: {
        text_drawBigChar(renderer, indexes_charToIdx('='), 2,
                         16 * (4 + localCurrentCollumn), y, visual_whiteText,
                         (rowSeleted && selectedVariable == 0) ||
                             (isAudioPlaying && cursorY == rowIndex));
        text_drawBigChar(renderer, indexes_charToIdx('='), 2,
                         16 * (5 + localCurrentCollumn), y, visual_whiteText,
                         (rowSeleted && selectedVariable == 1) ||
                             (isAudioPlaying && cursorY == rowIndex));
        localCurrentCollumn += 3;
        if (currentViewMode >= 1) {
          text_drawBigChar(renderer, indexes_charToIdx('.'), 2,
                           16 * (4 + localCurrentCollumn), y, visual_greenText,
                           (rowSeleted && selectedVariable == 2) ||
                               (isAudioPlaying && cursorY == rowIndex));
          text_drawBigChar(renderer, indexes_charToIdx('.'), 2,
                           16 * (5 + localCurrentCollumn), y, visual_greenText,
                           (rowSeleted && selectedVariable == 3) ||
                               (isAudioPlaying && cursorY == rowIndex));
          localCurrentCollumn += 3;
        }
        break;
      }
      }

      for (unsigned char i = 0;
           i < std::max(currentViewMode - 1, 0) && i < r->effects.size(); i++) {
        char effect_number = '?';
        bool effect_autoreset = false;
        effect e = r->effects.at(i);
        switch (e.type) {
        case effectTypes::null: {
          effect_number = '-';
          break;
        }
        case effectTypes::arpeggio: {
          effect_number = '0';
          break;
        }
        case effectTypes::pitchUp: {
          effect_number = '1';
          effect_autoreset = true;
          break;
        }
        case effectTypes::pitchDown: {
          effect_number = '2';
          effect_autoreset = true;
          break;
        }
        case effectTypes::volumeUp: {
          effect_number = '5';
          effect_autoreset = true;
          break;
        }
        case effectTypes::volumeDown: {
          effect_number = '6';
          effect_autoreset = true;
          break;
        }
        case effectTypes::instrumentVariation: {
          effect_number = 'C';
          break;
        }
        default:
          break;
        }
        bool effectIsNull = e.type == effectTypes::null;
        text_drawBigChar(renderer, indexes_charToIdx(effect_number), 2,
                         16 * (4 + localCurrentCollumn), y,
                         effectIsNull       ? visual_greyText
                         : effect_autoreset ? visual_greenText
                                            : visual_yellowText,
                         (rowSeleted && selectedVariable == 4 + (i * 5)) ||
                             (isAudioPlaying && cursorY == rowIndex));
        hex4(e.effect, letters.data());
        for (unsigned char hexNumberIndex = 0; hexNumberIndex < 4;
             hexNumberIndex++) {
          text_drawBigChar(renderer,
                           indexes_charToIdx(
                               effectIsNull ? '-' : letters.at(hexNumberIndex)),
                           2, 16 * (hexNumberIndex + 5 + localCurrentCollumn),
                           y,
                           effectIsNull       ? visual_greyText
                           : effect_autoreset ? visual_greenText
                                              : visual_yellowText,

                           (rowSeleted &&
                            selectedVariable == 5 + hexNumberIndex + (i * 5)) ||
                               (isAudioPlaying && cursorY == rowIndex));
        }
        localCurrentCollumn += 6;
      }
      if (currentViewMode >= 1)
        text_drawBigChar(renderer, indexes_charToIdx('\x1c'), 2,
                         16 * (3 + localCurrentCollumn), y, visual_greyText, 0);
      currentRow++;
    }
    currentCollumn += patternMenu_instrumentCollumnWidth[static_cast<size_t>(
        currentViewMode)];
  }
}

void instruments(SDL_Renderer *renderer, const int windowHeight,
                 orderStorage &orders, const unsigned int fontTileCountW,
                 const unsigned int fontTileCountH, orderIndexStorage &indexes,
                 instrumentStorage &instruments,
                 const CursorPos &cursorPosition) {
  text_drawText(renderer, "Instruments: ", 2, 0, 16, visual_whiteText, 0,
                fontTileCountW);
  unsigned char orderTableCount = orders.tableCount();
  unsigned char instrumentCount = instruments.inst_count();
  unsigned char orderIndexCount =
      indexes.instCount(SDL_max(orderTableCount, instrumentCount));
  unsigned char instCount =
      SDL_max(SDL_max(orderTableCount, instrumentCount), 0);
  if (instCount > 0)
    if (orderTableCount < instCount || instrumentCount < instCount ||
        orderIndexCount < instCount) {
      cmd::log::warning("Corrected instrument length inconsistencies");
      while (orders.tableCount() < instCount)
        orders.at(orders.addTable())->add_order();
      while (instruments.inst_count() < instCount)
        instruments.add_inst(audioChannelType::null);
      while (indexes.instCount(instCount) < instCount)
        indexes.addInst();
    }
  std::string numberStr;
  visual_numberToString(numberStr.data(), instCount);
  text_drawText(renderer, numberStr.c_str(), 2, 13 * 16, 16, visual_whiteText,
                0, fontTileCountW);
  text_drawText(renderer, "Z to add instrument", 2, 0, 32, visual_whiteText, 0,
                fontTileCountW);
  text_drawText(renderer, "X to remove selected instrument", 2, 0, 48,
                visual_whiteText, 0, fontTileCountW);
  text_drawText(renderer, "C to change instrument type", 2, 0, 64,
                visual_whiteText, 0, fontTileCountW);
  unsigned char startingRow = static_cast<unsigned char>(
      std::max(0, static_cast<short>(cursorPosition.y) -
                      static_cast<unsigned char>(fontTileCountH / 2)));
  unsigned char currentRow = 0;
  for (unsigned char i = startingRow; i < instCount; i++) {
    unsigned short y = 16 * (static_cast<unsigned short>(currentRow) + 6);
    if (y >= windowHeight)
      break;
    char letter1, letter2;
    hex2(i, letter1, letter2);

    text_drawBigChar(renderer, indexes_charToIdx(letter1), 2, 0, y,
                     visual_whiteText, i == cursorPosition.y);
    text_drawBigChar(renderer, indexes_charToIdx(letter2), 2, 16, y,
                     visual_whiteText, i == cursorPosition.y);
    text_drawText(renderer, getTypeName(instruments.at(i)->get_type(), false),
                  2, 48, y, visual_whiteText, i == cursorPosition.y,
                  fontTileCountW);
    currentRow++;
  }
}

void order_management(SDL_Renderer *renderer, const int windowWidth,
                      const int windowHeight, orderStorage &orders,
                      const unsigned int fontTileCountW,
                      const unsigned int fontTileCountH,
                      instrumentStorage &instruments,
                      const CursorPos &cursorPosition) {
  unsigned char startingRow = static_cast<unsigned char>(
      std::max(0, static_cast<short>(cursorPosition.y) -
                      static_cast<short>(fontTileCountH / 4)));
  unsigned char currentRow = 0;
  for (unsigned char i = startingRow; i < orders.tableCount(); i++) {
    unsigned short y = 16 * (static_cast<unsigned short>(currentRow) + 1);
    if (y >= windowHeight / 2)
      break;
    char letter1;
    char letter2;
    hex2(i, letter1, letter2);
    text_drawBigChar(renderer, indexes_charToIdx(letter1), 2, 0, y,
                     visual_whiteText,
                     i == (cursorPosition.subMenu ? cursorPosition.selection.y
                                                  : cursorPosition.y));
    text_drawBigChar(renderer, indexes_charToIdx(letter2), 2, 16, y,
                     visual_whiteText,
                     i == (cursorPosition.subMenu ? cursorPosition.selection.y
                                                  : cursorPosition.y));

    instrumentOrderTable *table = orders.at(i);
    unsigned char startingCollumn = static_cast<unsigned char>(
        std::max(0, static_cast<short>(cursorPosition.x) -
                        (static_cast<short>(fontTileCountW) - 4) / 6));
    unsigned char currentCollumn = 0;
    for (unsigned char j = startingCollumn; j < table->order_count(); j++) {
      unsigned short x =
          16 * (static_cast<unsigned short>(currentCollumn * 3) + 3);
      hex2(j, letter1, letter2);
      text_drawBigChar(renderer, indexes_charToIdx(letter1), 2, x, y,
                       visual_greenText,
                       cursorPosition.subMenu
                           ? (i == cursorPosition.selection.y &&
                              j == cursorPosition.selection.x)
                           : (i == cursorPosition.y && j == cursorPosition.x));
      text_drawBigChar(renderer, indexes_charToIdx(letter2), 2, x + 16, y,
                       visual_greenText,
                       cursorPosition.subMenu
                           ? (i == cursorPosition.selection.y &&
                              j == cursorPosition.selection.x)
                           : (i == cursorPosition.y && j == cursorPosition.x));
      currentCollumn++;
    }
    currentRow++;
  }

  int ceiling = fontTileCountH / 2 * 16;
  barrier(renderer, ceiling, windowWidth);
  if (cursorPosition.subMenu == 0) {
    text_drawText(renderer, "X to delete selected order", 2, 0, ceiling + 16,
                  visual_whiteText, 0, fontTileCountW);
    text_drawText(renderer, "C to clone selected order", 2, 0, ceiling + 32,
                  visual_whiteText, 0, fontTileCountW);
  } else {
    text_drawText(renderer, "Choose an instrument to clone to", 2, 0,
                  ceiling + 16, visual_whiteText, 0, fontTileCountW);
    std::string digits = "012";
    digits.at(2) = 0;
    hex2(cursorPosition.x, digits.at(0), digits.at(1));
    text_drawText(renderer, digits.c_str(), 2, 0, ceiling + 48,
                  visual_whiteText, 0, fontTileCountW);
    text_drawText(
        renderer,
        getTypeName(instruments.at(cursorPosition.x)->get_type(), false), 2, 48,
        ceiling + 48, visual_whiteText, 0, fontTileCountW);
  }
}

void options(SDL_Renderer *renderer, const unsigned int fontTileCountW,
             const CursorPos &cursorPosition, const unsigned short tempo,
             const unsigned short patternLength) {
  text_drawText(renderer, "W to increase", 2, 0, 16, visual_whiteText, 0,
                fontTileCountW);
  text_drawText(renderer, "S to decrease", 2, 0, 32, visual_whiteText, 0,
                fontTileCountW);
  text_drawText(renderer, "Rows per minute", 2, 0, 64, visual_whiteText,
                cursorPosition.y == 0, fontTileCountW);
  text_drawText(renderer, "Rows per order", 2, 0, 80, visual_whiteText,
                cursorPosition.y == 1, fontTileCountW);
  std::string numbers = "123456";
  visual_numberToString(numbers.data(), tempo);
  text_drawText(renderer, numbers.c_str(), 2, 256, 64, visual_whiteText,
                cursorPosition.y == 0, fontTileCountW);
  visual_numberToString(numbers.data(), patternLength);
  text_drawText(renderer, numbers.c_str(), 2, 256, 80, visual_whiteText,
                cursorPosition.y == 1, fontTileCountW);
}

void file(SDL_Renderer *renderer, const int windowWidth, const int windowHeight,
          const unsigned int fontTileCountW, const unsigned int fontTileCountH,
          const CursorPos &cursorPosition, char *&errorText,
          const std::filesystem::path &fileMenuDirectory) {
  if (errorText[0] == 0) {
    text_drawText(renderer, "Welcome to the FILE PICKER", 2, 0, 16,
                  visual_whiteText, 0, fontTileCountW);
  } else {
    text_drawText(renderer, errorText, 2, 0, 16, visual_redText, 0,
                  fontTileCountW);
  }
  barrier(renderer, 32, windowWidth);
  text_drawText(renderer,
                "S to save a file\n"
                "Return to load a file\n"
                "R to render\n"
                "ESC to go to parent directory\n"
#ifdef _WIN32
                "A-Q to navigate to respective drive",
#else
                ,
#endif
                2, 0, 48, visual_whiteText, 0, fontTileCountW);
  text_drawText(renderer, fileMenuDirectory.string().c_str(), 2, 0,
                128, // Converting to string and then char* to get around
                     // typing issues on Windows
                visual_whiteText, 0, INT_MAX);
  unsigned short y = 144;
  int i = 0;
  int initialEntry = std::max(0, static_cast<int>(cursorPosition.y) -
                                     static_cast<int>(fontTileCountH / 2));
  try {
    if (std::filesystem::exists(fileMenuDirectory) &&
        std::filesystem::is_directory(fileMenuDirectory)) {
      // Iterate through each file in the directory
      for (const auto &entry :
           std::filesystem::directory_iterator(fileMenuDirectory)) {
        if (entry.path().filename().string()[0] == '.')
          continue;
        if (i >= initialEntry) {
          text_drawText(renderer, entry.path().filename().string().c_str(), 2,
                        0, y,
                        entry.is_directory() ? SDL_Color{63, 127, 255, 255}
                                             : visual_greenText,
                        i == static_cast<int>(cursorPosition.y), INT_MAX);
          y += 16;
        }
        i++;
        if (y >= windowHeight)
          break;
      }
    } else {
      errorText = const_cast<char *>("Directory not found or invalid.");
    }
  } catch (std::filesystem::filesystem_error &error) {
    errorText = const_cast<char *>("Filesystem error reading directory");
  }
  if (i >= initialEntry && y < windowHeight)
    text_drawText(renderer, "..", 2, 0, y, visual_yellowText,
                  i == static_cast<int>(cursorPosition.y), fontTileCountW);
}

void file_save_render(SDL_Renderer *renderer, const int windowHeight,
                      const unsigned int fontTileCountW,
                      const CursorPos &cursorPosition, const bool render,
                      const std::string &fileName,
                      const std::filesystem::path &fileMenuDirectory) {
  text_drawText(renderer, render ? "Rendering to" : "Saving to", 2, 0, 16,
                visual_whiteText, 0, fontTileCountW);
  text_drawText(renderer,
                const_cast<char *>(
                    (fileMenuDirectory.string() + PATH_SEPERATOR_S).c_str()),
                2, 160, 16, visual_whiteText, 0, fontTileCountW - 10);
  if (cursorPosition.subMenu == 1)
    text_drawText(renderer,
                  "That file exists, are you sure?\n"
                  "Enter - Yes, Escape - No.",
                  2, 0, windowHeight - 32, visual_redText, 0, fontTileCountW);
  text_drawText(renderer, "Type a filename:", 2, 0, 128, visual_whiteText, 0,
                fontTileCountW - 10);
  text_drawText(renderer, fileName.c_str(), 2, 0, 144, visual_whiteText, 0,
                fontTileCountW - 10);
  if (render)
    text_drawText(renderer, "Program may not respond during render", 2, 0,
                  windowHeight - 16, visual_yellowText, 0, fontTileCountW);
}

void log(SDL_Renderer *renderer, const int windowWidth, const int windowHeight,
         const unsigned int fontTileCountH, const CursorPos &cursorPosition,
         long millis) {
  size_t logCount = cmd::log::logs.size();
  for (size_t i = 0; i < fontTileCountH * 2; i++) {
    size_t index = i + cursorPosition.y;
    ssize_t logIndex = logCount - index - 1;
    if (logIndex < 0)
      break;
    int y = i * 8 + 16;
    if (y >= windowHeight)
      break;
    struct log &l = cmd::log::logs.at(logIndex);
    bool invert = l.printed && (millis & 1024) == 1024;
    switch (l.severity) {
    case 0: {
      text_drawText(renderer, "DEBUG", 1, 0, y, visual_whiteText, invert, 5);
      break;
    }
    case 1: {
      text_drawText(renderer, "NOTE ", 1, 0, y, visual_blueText, invert, 5);
      break;
    }
    case 2: {
      text_drawText(renderer, " WARN", 1, 0, y, visual_yellowText, invert, 5);
      break;
    }
    case 3: {
      text_drawText(renderer, "ERROR", 1, 0, y, visual_redText, invert, 5);
      break;
    }
    case 4: {
      text_drawText(renderer, "CRIT!", 1, 0, y, visual_magentaText, invert, 5);
      break;
    }
    default:
      break;
    }
    text_drawText(renderer, l.msg.c_str(), 1, 48, y,
                  l.printed ? visual_whiteText : visual_greyText, 0,
                  windowWidth / 8 - 6);
  }
}
} // namespace guiMenus

void screenUpdate(SDL_Renderer *renderer, SDL_Window *window,
                  int &lastWindowWidth, int &lastWindowHeight,
                  const GlobalMenus currentMenu, const bool isAudioPlaying,
                  std::array<Sint16, WAVEFORM_SAMPLE_COUNT> waveformDisplay,
                  const bool hasUnsavedChanges, const CursorPos &cursorPosition,
                  orderIndexStorage &indexes,
                  const unsigned short currentPattern,
                  const unsigned char currentRow, orderStorage &orders,
                  const unsigned short currentlyViewedOrder,
                  const char currentViewMode, instrumentStorage &instruments,
                  const unsigned short tempo,
                  const unsigned short patternLength, char *&errorText,
                  const std::filesystem::path &fileMenuDirectory,
                  const std::string saveFileName,
                  const std::string renderFileName,
                  const std::filesystem::path &docPath) {
  long millis = SDL_GetTicks64();
  int windowWidth, windowHeight;
  SDL_GetWindowSize(window, &windowWidth, &windowHeight);
  if (lastWindowWidth != windowWidth || lastWindowHeight != windowHeight) {
    lastWindowWidth = windowWidth;
    lastWindowHeight = windowHeight;
  }
  unsigned int windowHorizontalTileCount =
      static_cast<unsigned int>(SDL_ceil(windowWidth / TILE_SIZE_F));
  unsigned int windowVerticalTileCount =
      static_cast<unsigned int>(SDL_ceil(windowHeight / TILE_SIZE_F));
  unsigned int fontTileCountH = windowHeight / 16 - 1;
  unsigned int fontTileCountW = windowWidth / 16;
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  // int x = millis%windowWidth;
  // int y = (millis/windowWidth)%windowHeight;

  guiFunc::background(renderer, windowHorizontalTileCount,
                      windowVerticalTileCount, millis);
  if (currentMenu == GlobalMenus::main_menu) {
    guiFunc::titleScreen(renderer, windowWidth, windowHeight, millis,
                         fontTileCountW);
  } else {
    guiFunc::topBar(renderer, windowWidth, windowHeight, waveformDisplay,
                    isAudioPlaying, hasUnsavedChanges, currentMenu);
    switch (currentMenu) {
    case GlobalMenus::main_menu:
      text_drawText(renderer, "Error", 3, 18, 18, visual_redText, 1, 10);
      break;
    case GlobalMenus::help_menu:
      guiMenus::help(renderer, docPath, cursorPosition, windowHeight);
      break;
    case GlobalMenus::order_menu:
      guiMenus::order(renderer, cursorPosition, windowWidth, windowHeight,
                      fontTileCountW, fontTileCountH, indexes);
      break;
    case GlobalMenus::pattern_menu:
      guiMenus::pattern(renderer, windowWidth, windowHeight, orders,
                        fontTileCountW, fontTileCountH, indexes, isAudioPlaying,
                        currentPattern, currentlyViewedOrder, instruments,
                        currentRow, cursorPosition, currentViewMode);
      break;
    case GlobalMenus::instrument_menu:
      guiMenus::instruments(renderer, windowHeight, orders, fontTileCountW,
                            fontTileCountH, indexes, instruments,
                            cursorPosition);
      break;
    case GlobalMenus::order_management_menu:
      guiMenus::order_management(renderer, windowWidth, windowHeight, orders,
                                 fontTileCountW, fontTileCountH, instruments,
                                 cursorPosition);
      break;
    case GlobalMenus::options_menu:
      guiMenus::options(renderer, fontTileCountW, cursorPosition, tempo,
                        patternLength);
      break;
    case GlobalMenus::file_menu:
      guiMenus::file(renderer, windowWidth, windowHeight, fontTileCountW,
                     fontTileCountH, cursorPosition, errorText,
                     fileMenuDirectory);
      break;
    case GlobalMenus::save_file_menu:
      guiMenus::file_save_render(renderer, windowHeight, fontTileCountW,
                                 cursorPosition, false, saveFileName,
                                 fileMenuDirectory);
      break;
    case GlobalMenus::render_menu:
      guiMenus::file_save_render(renderer, windowHeight, fontTileCountW,
                                 cursorPosition, true, renderFileName,
                                 fileMenuDirectory);
      break;
    case GlobalMenus::quit_confirmation_menu: {
      text_drawText(renderer, "Unsaved changes", 2,
                    (windowWidth - (15 * 16)) / 2, windowHeight / 2 - 16,
                    visual_redText, 0, fontTileCountW);
      text_drawText(renderer, "Press ESC again to quit anyway", 2,
                    (windowWidth - (30 * 16)) / 2, windowHeight / 2,
                    visual_whiteText, 0, fontTileCountW);
      text_drawText(renderer, "Press F7 to go to file menu", 2,
                    (windowWidth - (27 * 16)) / 2, windowHeight / 2 + 16,
                    visual_whiteText, 0, fontTileCountW);
      break;
    }
    case GlobalMenus::log_menu:
      guiMenus::log(renderer, windowWidth, windowHeight, fontTileCountH,
                    cursorPosition, millis); break;
    case GlobalMenus::debug_menu: {
      text_drawText(renderer, "DEBUG MENU", 4, 0, 16, visual_greenText, 0,
                    fontTileCountW / 2);
      text_drawText(renderer,
                    "[O]ut_of_range  exception\n"
                    "[L]ogic_error   exception\n"
                    "[R]untime_error exception",
                    2, 0, 48, visual_redText, 0, fontTileCountW);
      text_drawText(renderer,
                    "[N]otice [W]arning [E]rror [C]ritical"
                    "(un)[F]reeze audio",
                    2, 0, 96, visual_whiteText, 0, fontTileCountW);
      text_drawText(renderer,
                    "[D]ismantle an order table\n"
                    "d[I]smantle an index row\n"
                    "di[S]mantle an instrument",
                    2, 0, 118, visual_yellowText, 0, fontTileCountW);
      break;
    }
    }
  }
}
