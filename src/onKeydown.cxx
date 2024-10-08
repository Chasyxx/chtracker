/*
  Copyright (C) 2024 Chase Taylor

  This file is part of chTRACKER at path ./src/onKeydown.cxx

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
 *   All #include directives go here   *
 *                                     *
 ***************************************/

#include "channel.hxx"
#include "log.hxx"
#include "main.h"
#include "order.hxx"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_scancode.h>
#include <filesystem>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <commdlg.h>
#include <windows.h>
#endif

/*************************************
 *                                    *
 *            MACRO SECTION           *
 *   All #define DIRECTIVES GO HERE   *
 *                                    *
 *************************************/

// none

/****************************
 *                           *
 *     FUNCTIONS SECTION     *
 *   ALL FUNCTIONS GO HERE   *
 *                           *
 ****************************/

void onOpenMenu(CursorPos &p) {
  p = {.x = 0, .y = 0, .subMenu = 0, .selection = {.x = 0, .y = 0}};
}

#ifndef IN_CHTRACKER_CONTEXT
int saveFile(std::filesystem::path);
int loadFile(std::filesystem::path);
int renderTo(std::filesystem::path);
#endif

void onSDLKeyDown(const SDL_Event *event, int &quit, GlobalMenus &currentMenu,
                  CursorPos &cursorPosition, std::string &saveMenuFilename,
                  std::string &renderMenuFilename,
                  std::filesystem::path &fileMenuPath, char *&fileMenuError,
                  bool &hasUnsavedChanges, const unsigned int limitX,
                  const unsigned int limitY, bool &freezeAudio,
                  bool &audioIsFrozen, unsigned short &currentlyViewedOrder,
                  char &viewMode, bool &playAudio,
                  instrumentStorage &instrumentSystem,
                  orderIndexStorage &indexes, orderStorage &orders,
                  const unsigned short audio_pattern,
                  unsigned short &patternLength, const Uint8 *currentKeyStates,
                  unsigned short &audio_tempo) {
  SDL_Keysym ks = event->key.keysym;
  SDL_Keycode code = ks.sym;
  /********************************
   *                               *
   *     Filename editor binds     *
   *                               *
   ********************************/
  if (currentMenu == GlobalMenus::save_file_menu ||
      currentMenu == GlobalMenus::render_menu) {
    if (code == SDLK_ESCAPE) {
      currentMenu = GlobalMenus::file_menu;
      onOpenMenu(cursorPosition);
    }
    if (code == SDLK_BACKSPACE) {
      if (currentMenu == GlobalMenus::save_file_menu &&
          !saveMenuFilename.empty())
        saveMenuFilename.pop_back();
      else if (!renderMenuFilename.empty())
        renderMenuFilename.pop_back();
    }
    if (code == SDLK_RETURN || code == SDLK_RETURN2) {
      std::filesystem::path path(fileMenuPath.string() + PATH_SEPERATOR_S +
                                 (currentMenu == GlobalMenus::render_menu
                                      ? renderMenuFilename
                                      : saveMenuFilename));
      try {
        if (std::filesystem::exists(path) && cursorPosition.subMenu == 0) {
          cursorPosition.subMenu = 1;
          return;
        }
        if (currentMenu == GlobalMenus::save_file_menu) {
          int exit_code = saveFile(path);
          if (exit_code) {
            fileMenuError = const_cast<char *>("Refused to save the file");
            currentMenu = GlobalMenus::file_menu;
            onOpenMenu(cursorPosition);
          } else {
            currentMenu = GlobalMenus::pattern_menu;
            onOpenMenu(cursorPosition);
            hasUnsavedChanges = false;
          };
        } else {
          int exit_code = renderTo(path);
          if (exit_code) {
            fileMenuError = const_cast<char *>("Refused to render to the file");
            currentMenu = GlobalMenus::file_menu;
            onOpenMenu(cursorPosition);
          } else {
            currentMenu = GlobalMenus::pattern_menu;
            onOpenMenu(cursorPosition);
          };
        }
      } catch (std::filesystem::filesystem_error &) {
        fileMenuError = const_cast<char *>(
            "Filesystem error trying to save/render to the file");
        currentMenu = GlobalMenus::file_menu;
        onOpenMenu(cursorPosition);
      }
    }
    return;
  }
  /***************************
   *                          *
   *     Debug menu binds     *
   *                          *
   ***************************/
  if (currentMenu == GlobalMenus::debug_menu) {
    switch (code) {
    case 'o': {
      throw std::out_of_range("Requested in debug menu");
      break;
    }
    case 'l': {
      throw std::logic_error("Requested in debug menu");
      break;
    }
    case 'r': {
      throw std::runtime_error("Requested in debug menu");
      break;
    }
    case 'n': {
      cmd::log::notice("Debug menu notice log");
      break;
    }
    case 'w': {
      cmd::log::warning("Debug menu warning log");
      break;
    }
    case 'e': {
      cmd::log::error("Debug menu error log");
      break;
    }
    case 'c': {
      cmd::log::critical("Debug menu critical log");
      break;
    }
    case 'f': {
      cmd::log::warning("Debug menu froze/unfroze audio");
      freezeAudio = !freezeAudio;
      break;
    }
    case 'd': {
      cmd::log::warning("Debug menu dismantled an order table");
      orders.removeTable(orders.tableCount() - 1);
      break;
    }
    case 'i': {
      cmd::log::warning("Debug menu dismantled an index row");
      indexes.removeInst(indexes.instCount(0) - 1);
      break;
    }
    case 's': {
      cmd::log::warning("Debug menu dismantled an instrument");
      instrumentSystem.remove_inst(instrumentSystem.inst_count() - 1);
      break;
    }
    default:
      break;
    }
  }
  /*****************************
   *                            *
   *     Title screen binds     *
   *                            *
   *****************************/
  if (currentMenu == GlobalMenus::main_menu) {
    switch (code) {
    case SDLK_ESCAPE: {
      quit = 1;
      break;
    }
    case 'z': {
      currentMenu = GlobalMenus::help_menu;
      break;
    }
    }
    return;
  }
  /***********************
   *                      *
   *     Global binds     *
   *                      *
   ***********************/
  switch (code) {
  case SDLK_ESCAPE: {
    if (currentMenu == GlobalMenus::file_menu) {
      if (std::filesystem::path(fileMenuPath).has_parent_path()) {
        fileMenuPath =
            std::filesystem::path(fileMenuPath).parent_path().string();
        cursorPosition.y = 0;
      }
      break;
    }
    quit = 1;
    break;
  }
  case SDLK_F1: {
    currentMenu = GlobalMenus::help_menu;
    onOpenMenu(cursorPosition);
    break;
  }
  case SDLK_F2: {
    currentMenu = GlobalMenus::order_menu;
    onOpenMenu(cursorPosition);
    break;
  }
  case SDLK_F3: {
    currentMenu = GlobalMenus::pattern_menu;
    onOpenMenu(cursorPosition);
    break;
  }
  case SDLK_F4: {
    currentMenu = GlobalMenus::instrument_menu;
    onOpenMenu(cursorPosition);
    break;
  }
  case SDLK_F5: {
    currentMenu = GlobalMenus::order_management_menu;
    onOpenMenu(cursorPosition);
    break;
  }
  case SDLK_F6: {
    currentMenu = GlobalMenus::options_menu;
    onOpenMenu(cursorPosition);
    break;
  }
  case SDLK_F7: {
    currentMenu = GlobalMenus::file_menu;
    onOpenMenu(cursorPosition);
    break;
  }
  case SDLK_F8: {
    currentMenu = GlobalMenus::log_menu;
    onOpenMenu(cursorPosition);
    break;
  }
  case SDLK_F12: {
    if (currentKeyStates[SDL_SCANCODE_F11] &&
        currentKeyStates[SDL_SCANCODE_F10]) {
      currentMenu = GlobalMenus::debug_menu;
      onOpenMenu(cursorPosition);
    }
    break;
  }
  case SDLK_UP: {
    if (cursorPosition.y > 0)
      cursorPosition.y--;
    break;
  }
  case SDLK_DOWN: {
    if (cursorPosition.y < limitY)
      cursorPosition.y++;
    break;
  }
  case SDLK_LEFT: {
    if (cursorPosition.x > 0)
      cursorPosition.x--;
    break;
  }
  case SDLK_RIGHT: {
    if (cursorPosition.x < limitX)
      cursorPosition.x++;
    break;
  }
  case SDLK_RETURN:
  case SDLK_RETURN2: {
    if (currentMenu == GlobalMenus::file_menu) {
      int i = 0;
      bool found = false;
      fileMenuError = const_cast<char *>("");
      try {
        if (std::filesystem::exists(fileMenuPath) &&
            std::filesystem::is_directory(fileMenuPath)) {
          // Iterate through each file in the directory
          for (const auto &entry :
               std::filesystem::directory_iterator(fileMenuPath)) {
            if (entry.path().filename().string()[0] == '.')
              continue;
            if (i++ == static_cast<int>(cursorPosition.y)) {
              found = true;
              if (entry.is_directory()) {
                std::filesystem::path newPath =
                    fileMenuPath / entry.path().filename();
                if (std::filesystem::exists(fileMenuPath)) {
                  fileMenuPath = newPath;
                }
                cursorPosition.y = 0;
              } else {
                freezeAudio = true;
                while (!audioIsFrozen) {
                };
                if (loadFile(entry.path())) {
                  if (SDL_strlen(fileMenuError) == 0)
                    fileMenuError =
                        const_cast<char *>("Refused to load the file");
                } else {
                  currentlyViewedOrder = 0;
                  viewMode = 3;
                  currentMenu = GlobalMenus::pattern_menu;
                  onOpenMenu(cursorPosition);
                  hasUnsavedChanges = false;
                };
                freezeAudio = false;
              }
              break;
            }
          }
        }
      } catch (std::filesystem::filesystem_error &) {
        cursorPosition.y = 0;
      }
      if (!found) {
        if (std::filesystem::path(fileMenuPath).has_parent_path()) {
          fileMenuPath =
              std::filesystem::path(fileMenuPath).parent_path().string();
          cursorPosition.y = 0;
        }
      }
      break;
    }
    if (!freezeAudio) {
      playAudio = !playAudio;
      if (playAudio) {
        for (unsigned char i = 0; i < instrumentSystem.inst_count(); i++) {
          unsigned char patternIndex = indexes.at(audio_pattern)->at(i);
          row *currentRow = orders.at(i)->at(patternIndex)->at(0);
          instrumentSystem.at(i)->set_row(*currentRow);
        }
      }
    }
    break;
  }
  }
  /*************************************************
   *                                               *
   *     CTRL-O, CTRL-S, and CTRL-R on Windows     *
   *                                               *
   *************************************************/
  // TODO: Someone help me figure out how to do this on Linux.
  // I can't use GTK because i already use SDL with it's own main loop.
#ifdef _WIN32
  if ((currentKeyStates[SDL_SCANCODE_LCTRL] ||
       currentKeyStates[SDL_SCANCODE_RCTRL])) {
    if (code == 's') {
      OPENFILENAME ofn;
      char szFile[260] = {0};

      ZeroMemory(&ofn, sizeof(ofn));
      ofn.lStructSize = sizeof(ofn);
      ofn.hwndOwner = nullptr;
      ofn.lpstrFile = szFile;
      ofn.nMaxFile = sizeof(szFile);
      ofn.lpstrFilter = "chTRACKER modules\0*.CHT\0All files\0*.*\0";
      ofn.nFilterIndex = 1;
      ofn.lpstrFileTitle = nullptr;
      ofn.nMaxFileTitle = 0;
      ofn.lpstrInitialDir = nullptr;
      ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

      if (GetSaveFileName(&ofn) == TRUE) {
        if(saveFile(ofn.lpstrFile) == 0) {
          currentMenu = GlobalMenus::pattern_menu;
          onOpenMenu(cursorPosition);
        } else if (fileMenu_errorText[0] == 0) {
          fileMenu_errorText = const_cast<char*>("Couldn't save file");
          currentMenu = GlobalMenus::file_menu;
          onOpenMenu(cursorPosition);
        };
      }
    } else if (code == 'o') {
      OPENFILENAME ofn;
      char szFile[260] = { 0 };

      ZeroMemory(&ofn, sizeof(ofn));
      ofn.lStructSize = sizeof(ofn);
      ofn.hwndOwner = nullptr;
      ofn.lpstrFile = szFile;
      ofn.nMaxFile = sizeof(szFile);
      ofn.lpstrFilter = "chTRACKER modules\0*.CHT\0All files\0*.*\0";
      ofn.nFilterIndex = 1;
      ofn.lpstrFileTitle = nullptr;
      ofn.nMaxFileTitle = 0;
      ofn.lpstrInitialDir = nullptr;
      ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

      if (GetOpenFileName(&ofn) == TRUE) {
        if(loadFile(ofn.lpstrFile) == 0) {
          currentMenu = GlobalMenus::pattern_menu;
          onOpenMenu(cursorPosition);
        } else if (fileMenu_errorText[0] == 0) {
          fileMenu_errorText = const_cast<char*>("Couldn't open file");
          currentMenu = GlobalMenus::file_menu;
          onOpenMenu(cursorPosition);
        };
      }
    } else if (code == 'r') {
      OPENFILENAME ofn;
      char szFile[260] = {0};

      ZeroMemory(&ofn, sizeof(ofn));
      ofn.lStructSize = sizeof(ofn);
      ofn.hwndOwner = nullptr;
      ofn.lpstrFile = szFile;
      ofn.nMaxFile = sizeof(szFile);
      ofn.lpstrFilter = "WAV files\0*.WAV\0All files\0*.*\0";
      ofn.nFilterIndex = 1;
      ofn.lpstrFileTitle = nullptr;
      ofn.nMaxFileTitle = 0;
      ofn.lpstrInitialDir = nullptr;
      ofn.lpstrTitle = "Render to";
      ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

      if (GetSaveFileName(&ofn) == TRUE) {
        if(renderTo(ofn.lpstrFile) == 0) {
          currentMenu = GlobalMenus::pattern_menu;
          onOpenMenu(cursorPosition);
        } else if (fileMenu_errorText[0] == 0) {
          fileMenu_errorText = const_cast<char*>("Couldn't render to the file");
          currentMenu = GlobalMenus::file_menu;
          onOpenMenu(cursorPosition);
        };
      }
    }
  }
#endif
  /******************************
   *                            *
   *     Pattern menu binds     *
   *                            *
   *****************************/
  if (currentMenu == GlobalMenus::pattern_menu) {
    switch (code) {
    case 'p': {
      if (orders.tableCount() == 0)
        currentlyViewedOrder = 0;
      else
        currentlyViewedOrder =
            std::max(0, static_cast<int>(currentlyViewedOrder) - 1);
      break;
    }
    case 'n': {
      if (orders.tableCount() == 0)
        currentlyViewedOrder = 0;
      else
        currentlyViewedOrder =
            SDL_min(indexes.rowCount() - 1, currentlyViewedOrder + 1);
      break;
    }
    case 'v': {
      if (viewMode < 5)
        viewMode++;
      break;
    }
    case 'u': {
      if (viewMode > 0) {
        viewMode--;
        cursorPosition.x = 0;
      }
      break;
    }
    }
    if (orders.tableCount() < 1)
      return;
    unsigned char selectedInstrument =
        cursorPosition.x /
        patternMenu_instrumentVariableCount[static_cast<size_t>(viewMode)];
    unsigned char selectedVariable =
        cursorPosition.x %
        patternMenu_instrumentVariableCount[static_cast<size_t>(viewMode)];
    row *r = orders.at(selectedInstrument)
                 ->at(indexes.at(currentlyViewedOrder)->at(selectedInstrument))
                 ->at(cursorPosition.y);
    if (selectedVariable == 0) {
      bool moveDown = true;
      switch (code) {
      case '0':
        r->feature = rowFeature::note;
        r->note = 'A';
        break;
      case '1':
        r->feature = rowFeature::note;
        r->note = 'B';
        break;
      case '2':
        r->feature = rowFeature::note;
        r->note = 'C';
        break;
      case '3':
        r->feature = rowFeature::note;
        r->note = 'D';
        break;
      case '4':
        r->feature = rowFeature::note;
        r->note = 'E';
        break;
      case '5':
        r->feature = rowFeature::note;
        r->note = 'F';
        break;
      case '6':
        r->feature = rowFeature::note;
        r->note = 'G';
        break;
      case '7':
        r->feature = rowFeature::note;
        r->note = 'H';
        break;
      case '8':
        r->feature = rowFeature::note;
        r->note = 'I';
        break;
      case '9':
        r->feature = rowFeature::note;
        r->note = 'J';
        break;
      case 'a':
        r->feature = rowFeature::note;
        r->note = 'K';
        break;
      case 'b':
        r->feature = rowFeature::note;
        r->note = 'L';
        break;
      case '-':
        r->feature = rowFeature::empty;
        r->octave = 4;
        break;
      case '=':
        r->feature = rowFeature::note_cut;
        r->octave = 4;
        break;
      default:
        moveDown = false;
        break;
      }
      if (moveDown) {
        cursorPosition.y++;
        hasUnsavedChanges = true;
      }
    } else if (selectedVariable == 1) {
      bool moveDown = true;
      switch (code) {
      case '0':
        r->octave = 0;
        break;
      case '1':
        r->octave = 1;
        break;
      case '2':
        r->octave = 2;
        break;
      case '3':
        r->octave = 3;
        break;
      case '4':
        r->octave = 4;
        break;
      case '5':
        r->octave = 5;
        break;
      case '6':
        r->octave = 6;
        break;
      case '7':
        r->octave = 7;
        break;
      case '8':
        r->octave = 8;
        break;
      case '9':
        r->octave = 9;
        break;
      case '-':
        r->feature = rowFeature::empty;
        r->octave = 4;
        break;
      case '=':
        r->feature = rowFeature::note_cut;
        r->octave = 4;
        break;
      default:
        moveDown = false;
        break;
      }
      if (moveDown) {
        cursorPosition.y++;
        hasUnsavedChanges = true;
      }
    } else if (selectedVariable < 4) {
      if ((code >= '0' && code <= '9') || (code >= 'a' && code <= 'f')) {
        unsigned char value;
        if (code >= 'a') {
          value = code - 'a' + 10;
        } else
          value = code - '0';
        if (selectedVariable == 2) {
          r->volume = (r->volume & 0x0F) | (value << 4);
          cursorPosition.x++;
        } else {
          r->volume = (r->volume & 0xF0) | value;
          cursorPosition.y++;
          cursorPosition.x--;
        }
        hasUnsavedChanges = true;
      }
    } else {
      unsigned char idx = (selectedVariable - 4) % 5;
      unsigned char effectIdx = (selectedVariable - 4) / 5;
      effect &e = r->effects.at(effectIdx);
      bool isEffectHead = idx == 0;
      if (isEffectHead) {
        bool moveDown = true;
        switch (code) {
        case '-':
          e.type = effectTypes::null;
          e.effect = 0;
          break;
        case '0':
          e.type = effectTypes::arpeggio;
          break;
        case '1':
          e.type = effectTypes::pitchUp;
          break;
        case '2':
          e.type = effectTypes::pitchDown;
          break;
        case '5':
          e.type = effectTypes::volumeUp;
          break;
        case '6':
          e.type = effectTypes::volumeDown;
          break;
        case 'c':
          e.type = effectTypes::instrumentVariation;
          break;
        default:
          moveDown = false;
          break;
        }
        if (moveDown) {
          cursorPosition.y++;
          hasUnsavedChanges = true;
        }
      } else if ((code >= '0' && code <= '9') || (code >= 'a' && code <= 'f')) {
        unsigned char value;
        if (code >= 'a') {
          value = code - 'a' + 10;
        } else
          value = code - '0';
        unsigned char shift = (4 - idx) << 2;
        e.effect = (e.effect & ~(static_cast<unsigned short>(0xF) << shift)) |
                   value << shift;
        if (idx == 4) {
          cursorPosition.y++;
          cursorPosition.x -= 3;
        } else
          cursorPosition.x++;
        hasUnsavedChanges = true;
      }
    }
    return;
  }
  /***************************************
   *                                      *
   *     Individual non-pattern binds     *
   *                                      *
   ***************************************/
  switch (code) {
  case 'z': {
    if (currentMenu == GlobalMenus::instrument_menu &&
        instrumentSystem.inst_count() < 254) {
      instrumentSystem.add_inst(audioChannelType::null);
      orders.at(orders.addTable())->add_order();
      indexes.addInst();
    } else if (currentMenu == GlobalMenus::order_menu) {
      indexes.addRow();
    } else
      break;
    hasUnsavedChanges = true;
    break;
  }
  case 'x': {
    if (currentMenu == GlobalMenus::instrument_menu &&
        instrumentSystem.inst_count() > 0) {
      instrumentSystem.remove_inst(cursorPosition.y);
      orders.removeTable(cursorPosition.y);
      indexes.removeInst(cursorPosition.y);
      if (orders.tableCount() == 0)
        currentlyViewedOrder = 0;
      hasUnsavedChanges = true;
    } else if (currentMenu == GlobalMenus::order_menu &&
               indexes.rowCount() > 1) {
      indexes.removeRow(cursorPosition.y);
      hasUnsavedChanges = true;
    } else if (currentMenu == GlobalMenus::order_management_menu &&
               cursorPosition.subMenu == 0 && orders.tableCount() > 0) {
      if (cursorPosition.x == 0)
        break;
      if (orders.at(cursorPosition.y)->order_count() < 2)
        break;
      for (unsigned short i = 0; i < indexes.rowCount(); i++) {
        unsigned char a = indexes.at(i)->at(cursorPosition.y);
        if (a > cursorPosition.x) {
          indexes.at(i)->decrement(cursorPosition.y);
        } else if (a == cursorPosition.x) {
          indexes.at(i)->set(cursorPosition.y, 0);
        }
      }

      orders.at(cursorPosition.y)->remove_order(cursorPosition.x);
      hasUnsavedChanges = true;
    }
    break;
  }
  case 'c': {
    if (currentMenu == GlobalMenus::instrument_menu &&
        instrumentSystem.inst_count() > 0) {
      instrumentSystem.at(cursorPosition.y)->cycle_type();
      hasUnsavedChanges = true;
    } else if (currentMenu == GlobalMenus::order_management_menu &&
               orders.tableCount() > 0) {
      if (cursorPosition.subMenu == 0) {
        cursorPosition.selection.x = cursorPosition.x;
        cursorPosition.selection.y = cursorPosition.y;
        cursorPosition.subMenu = 1;
        cursorPosition.y = 0;
        cursorPosition.x = 0;
        break;
      }
      order *output = orders.at(cursorPosition.x)
                          ->at(orders.at(cursorPosition.x)->add_order());
      order *input =
          orders.at(cursorPosition.selection.y)->at(cursorPosition.selection.x);
      for (int i = 0; i < patternLength; i++) {
        row *inputRow = input->at(i);
        row *outputRow = output->at(i);
        outputRow->feature = inputRow->feature;
        outputRow->note = inputRow->note;
        outputRow->octave = inputRow->octave;
        outputRow->volume = inputRow->volume;
        for (int j = 0;
             j < 4 && static_cast<size_t>(j) < inputRow->effects.size() &&
             static_cast<size_t>(j) < outputRow->effects.size();
             j++) {
          outputRow->effects.at(j).type = inputRow->effects.at(j).type;
          outputRow->effects.at(j).effect = inputRow->effects.at(j).effect;
        }
      }
      cursorPosition.selection.x = 0;
      cursorPosition.selection.y = 0;
      cursorPosition.subMenu = 0;
      cursorPosition.y = 0;
      cursorPosition.x = 0;
      hasUnsavedChanges = true;
    }
    break;
  }
  case 'w': {
    if (currentMenu == GlobalMenus::order_menu &&
        instrumentSystem.inst_count() > 0) {
      if (indexes.at(cursorPosition.y)->at(cursorPosition.x) < 254) {
        indexes.at(cursorPosition.y)->increment(cursorPosition.x);
        hasUnsavedChanges = true;
      }

      while (indexes.at(cursorPosition.y)->at(cursorPosition.x) >=
             orders.at(cursorPosition.x)->order_count())
        orders.at(cursorPosition.x)->add_order();
    } else if (currentMenu == GlobalMenus::options_menu) {
      if (cursorPosition.y == 0) {
        // RPM
        if ((currentKeyStates[SDL_SCANCODE_LSHIFT] ||
             currentKeyStates[SDL_SCANCODE_RSHIFT]) &&
            audio_tempo < 65525) {
          audio_tempo += 10;
          hasUnsavedChanges = true;
        } else if (audio_tempo < 65534) {
          audio_tempo++;
          hasUnsavedChanges = true;
        }
      } else {
        if (patternLength < 256) {
          patternLength++;
          orders.setRowCount(patternLength);
          hasUnsavedChanges = true;
        }
      }
    }
    break;
  }
  case 's': {
    if (currentMenu == GlobalMenus::order_menu &&
        instrumentSystem.inst_count() > 0) {
      if (indexes.at(cursorPosition.y)->at(cursorPosition.x) > 0) {
        indexes.at(cursorPosition.y)->decrement(cursorPosition.x);
        hasUnsavedChanges = true;
      }
    } else if (currentMenu == GlobalMenus::options_menu) {
      if (cursorPosition.y == 0) {
        // RPM
        if ((currentKeyStates[SDL_SCANCODE_LSHIFT] ||
             currentKeyStates[SDL_SCANCODE_RSHIFT]) &&
            audio_tempo > 40) {
          audio_tempo -= 10;
          hasUnsavedChanges = true;
        } else if (audio_tempo > 30) {
          audio_tempo--;
          hasUnsavedChanges = true;
        }
      } else {
        if (patternLength > 16) {
          patternLength--;
          orders.setRowCount(patternLength);
          hasUnsavedChanges = true;
        }
      }
    }
    break;
  }
  }
  /**************************************
   * Windows file menu drive navigation *
   **************************************/
#ifdef _WIN32
  if (currentMenu == GlobalMenus::file_menu && code >= 'a' && code <= 'q') {
    std::string path = "C:\\";
    path.at(0) = static_cast<char>(static_cast<char>(code) - 'a' + 'A');
    fileMenuPath = path;
  }
#endif
  return;
}