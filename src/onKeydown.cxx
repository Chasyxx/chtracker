#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>
#include <string>
#include <filesystem>
#include "main.h"
#include "order.hxx"
#include "channel.hxx"

void onOpenMenu(CursorPos &p) {
  p = {
      .x = 0, .y = 0, .subMenu = 0, .selection = {.x = 0, .y = 0}};
}

#ifndef IN_CHTRACKER_CONTEXT
int saveFile(std::filesystem::path);
int loadFile(std::filesystem::path);
int renderTo(std::filesystem::path);
#endif

void onSdlkeydown(SDL_Event *event, int &quit, GlobalMenus &global_currentMenu,
                      CursorPos &cursorPosition, std::string& saveFileMenu_fileName,
                      std::string& renderMenu_fileName, std::string& fileMenu_directoryPath,
                      char *fileMenu_errorText, bool &global_unsavedChanges,
                      const unsigned int limitX, const unsigned int limitY, bool &audio_freeze,
                      bool &audio_isFrozen, unsigned short &patternMenu_orderIndex,
                      char &patternMenu_viewMode, bool &audio_isPlaying,
                      instrumentStorage instrumentSystem, orderIndexStorage &indexes,
                      orderStorage &orders, unsigned short audio_pattern,
                      unsigned short &patternLength, const Uint8 *currentKeyStates,
                      unsigned short &audio_tenpo) {
    SDL_Keysym ks = event->key.keysym;
    SDL_Keycode code = ks.sym;
    if (global_currentMenu == GlobalMenus::save_file_menu ||
        global_currentMenu == GlobalMenus::render_menu) {
      if (code == SDLK_ESCAPE) {
        global_currentMenu = GlobalMenus::file_menu;
        onOpenMenu(cursorPosition);
      }
      if (code == SDLK_BACKSPACE && !saveFileMenu_fileName.empty()) {
        if (global_currentMenu == GlobalMenus::save_file_menu &&
            !saveFileMenu_fileName.empty())
          saveFileMenu_fileName.pop_back();
        else if (!renderMenu_fileName.empty())
          renderMenu_fileName.pop_back();
      }
      if (code == SDLK_RETURN || code == SDLK_RETURN2) {
        std::filesystem::path path(
            fileMenu_directoryPath + PATH_SEPERATOR_S +
            (global_currentMenu == GlobalMenus::render_menu
                 ? renderMenu_fileName
                 : saveFileMenu_fileName));
        try {
          if (std::filesystem::exists(path) && cursorPosition.subMenu == 0) {
            cursorPosition.subMenu = 1;
            return;
          }
          if (global_currentMenu == GlobalMenus::save_file_menu) {
            int exit_code = saveFile(path);
            if (exit_code) {
              fileMenu_errorText =
                  const_cast<char *>("Refused to save the file");
              global_currentMenu = GlobalMenus::file_menu;
              onOpenMenu(cursorPosition);
            } else {
              global_currentMenu = GlobalMenus::pattern_menu;
              onOpenMenu(cursorPosition);
              global_unsavedChanges = false;
            };
          } else {
            int exit_code = renderTo(path);
            if (exit_code) {
              fileMenu_errorText =
                  const_cast<char *>("Refused to render to the file");
              global_currentMenu = GlobalMenus::file_menu;
              onOpenMenu(cursorPosition);
            } else {
              global_currentMenu = GlobalMenus::pattern_menu;
              onOpenMenu(cursorPosition);
            };
          }
        } catch (std::filesystem::filesystem_error &) {
          fileMenu_errorText = const_cast<char *>(
              "Filesystem error trying to save/render to the file");
          global_currentMenu = GlobalMenus::file_menu;
          onOpenMenu(cursorPosition);
        }
      }
      return;
    }
    if (global_currentMenu == GlobalMenus::main_menu) {
      switch (code) {
      case SDLK_ESCAPE: {
        quit = 1;
        break;
      }
      case 'z': {
        global_currentMenu = GlobalMenus::help_menu;
        break;
      }
      }
    } else {
      switch (code) {
      case SDLK_ESCAPE: {
        if (global_currentMenu == GlobalMenus::file_menu) {
          if (std::filesystem::path(fileMenu_directoryPath).has_parent_path()) {
            fileMenu_directoryPath =
                std::filesystem::path(fileMenu_directoryPath)
                    .parent_path()
                    .string();
            cursorPosition.y = 0;
          }
          break;
        }
        quit = 1;
        break;
      }
      case SDLK_F1: {
        global_currentMenu = GlobalMenus::help_menu;
        onOpenMenu(cursorPosition);
        break;
      }
      case SDLK_F2: {
        global_currentMenu = GlobalMenus::order_menu;
        onOpenMenu(cursorPosition);
        break;
      }
      case SDLK_F3: {
        global_currentMenu = GlobalMenus::pattern_menu;
        onOpenMenu(cursorPosition);
        break;
      }
      case SDLK_F4: {
        global_currentMenu = GlobalMenus::instrument_menu;
        onOpenMenu(cursorPosition);
        break;
      }
      case SDLK_F5: {
        global_currentMenu = GlobalMenus::order_management_menu;
        onOpenMenu(cursorPosition);
        break;
      }
      case SDLK_F6: {
        global_currentMenu = GlobalMenus::options_menu;
        onOpenMenu(cursorPosition);
        break;
      }
      case SDLK_F7: {
        global_currentMenu = GlobalMenus::file_menu;
        onOpenMenu(cursorPosition);
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
        if (global_currentMenu == GlobalMenus::file_menu) {
          int i = 0;
          bool found = false;
          fileMenu_errorText = const_cast<char *>("");
          try {
            if (std::filesystem::exists(fileMenu_directoryPath) &&
                std::filesystem::is_directory(fileMenu_directoryPath)) {
              // Iterate through each file in the directory
              for (const auto &entry : std::filesystem::directory_iterator(
                       fileMenu_directoryPath)) {
                if (entry.path().filename().string()[0] == '.')
                  continue;
                if (i++ == static_cast<int>(cursorPosition.y)) {
                  found = true;
                  if (entry.is_directory()) {
                    std::string newPath;
                    if (fileMenu_directoryPath[fileMenu_directoryPath.length() -
                                               1] == PATH_SEPERATOR)
                      newPath = fileMenu_directoryPath +
                                entry.path().filename().string();
                    else
                      newPath = fileMenu_directoryPath + PATH_SEPERATOR_S +
                                entry.path().filename().string();
                    if (std::filesystem::exists(fileMenu_directoryPath)) {
                      fileMenu_directoryPath = newPath;
                    }
                    cursorPosition.y = 0;
                  } else {
                    audio_freeze = true;
                    while (!audio_isFrozen) {
                    };
                    if (loadFile(entry.path())) {
                      if (SDL_strlen(fileMenu_errorText) == 0)
                        fileMenu_errorText =
                            const_cast<char *>("Refused to load the file");
                    } else {
                      patternMenu_orderIndex = 0;
                      patternMenu_viewMode = 3;
                      global_currentMenu = GlobalMenus::pattern_menu;
                      onOpenMenu(cursorPosition);
                      global_unsavedChanges = false;
                    };
                    audio_freeze = false;
                  }
                  break;
                }
              }
            }
          } catch (std::filesystem::filesystem_error &) {
            cursorPosition.y = 0;
          }
          if (!found) {
            if (std::filesystem::path(fileMenu_directoryPath)
                    .has_parent_path()) {
              fileMenu_directoryPath =
                  std::filesystem::path(fileMenu_directoryPath)
                      .parent_path()
                      .string();
              cursorPosition.y = 0;
            }
          }
          break;
        }
        if (!audio_freeze) {
          audio_isPlaying = !audio_isPlaying;
          if (audio_isPlaying) {
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
      if (global_currentMenu == GlobalMenus::pattern_menu) {
        switch (code) {
        case 'p': {
          if (orders.tableCount() == 0)
            patternMenu_orderIndex = 0;
          else
            patternMenu_orderIndex =
                std::max(0, static_cast<int>(patternMenu_orderIndex) - 1);
          break;
        }
        case 'n': {
          if (orders.tableCount() == 0)
            patternMenu_orderIndex = 0;
          else
            patternMenu_orderIndex =
                SDL_min(indexes.rowCount() - 1, patternMenu_orderIndex + 1);
          break;
        }
        case 'v': {
          if (patternMenu_viewMode < 5)
            patternMenu_viewMode++;
          break;
        }
        case 'u': {
          if (patternMenu_viewMode > 0) {
            patternMenu_viewMode--;
            cursorPosition.x = 0;
          }
          break;
        }
        }
        if (orders.tableCount() < 1)
          return;
        unsigned char selectedInstrument =
            cursorPosition.x /
            patternMenu_instrumentVariableCount[static_cast<size_t>(
                patternMenu_viewMode)];
        unsigned char selectedVariable =
            cursorPosition.x %
            patternMenu_instrumentVariableCount[static_cast<size_t>(
                patternMenu_viewMode)];
        row *r =
            orders.at(selectedInstrument)
                ->at(indexes.at(patternMenu_orderIndex)->at(selectedInstrument))
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
          case 'c':
          case '2':
            r->feature = rowFeature::note;
            r->note = 'C';
            break;
          case 'd':
          case '3':
            r->feature = rowFeature::note;
            r->note = 'D';
            break;
          case 'e':
          case '4':
            r->feature = rowFeature::note;
            r->note = 'E';
            break;
          case 'f':
          case '5':
            r->feature = rowFeature::note;
            r->note = 'F';
            break;
          case 'g':
          case '6':
            r->feature = rowFeature::note;
            r->note = 'G';
            break;
          case 'h':
          case '7':
            r->feature = rowFeature::note;
            r->note = 'H';
            break;
          case 'i':
          case '8':
            r->feature = rowFeature::note;
            r->note = 'I';
            break;
          case 'j':
          case '9':
            r->feature = rowFeature::note;
            r->note = 'J';
            break;
          case 'k':
          case 'a':
            r->feature = rowFeature::note;
            r->note = 'K';
            break;
          case 'l':
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
            global_unsavedChanges = true;
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
            global_unsavedChanges = true;
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
            global_unsavedChanges = true;
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
              global_unsavedChanges = true;
            }
          } else if ((code >= '0' && code <= '9') ||
                     (code >= 'a' && code <= 'f')) {
            unsigned char value;
            if (code >= 'a') {
              value = code - 'a' + 10;
            } else
              value = code - '0';
            unsigned char shift = (4 - idx) << 2;
            e.effect =
                (e.effect & ~(static_cast<unsigned short>(0xF) << shift)) |
                value << shift;
            if (idx == 4) {
              cursorPosition.y++;
              cursorPosition.x -= 3;
            } else
              cursorPosition.x++;
            global_unsavedChanges = true;
          }
        }
      } else {
        switch (code) {
        case 'z': {
          if (global_currentMenu == GlobalMenus::instrument_menu &&
              instrumentSystem.inst_count() < 254) {
            instrumentSystem.add_inst(audioChannelType::null);
            orders.at(orders.addTable())->add_order();
            indexes.addInst();
          } else if (global_currentMenu == GlobalMenus::order_menu) {
            indexes.addRow();
          } else
            break;
          global_unsavedChanges = true;
          break;
        }
        case 'x': {
          if (global_currentMenu == GlobalMenus::instrument_menu &&
              instrumentSystem.inst_count() > 0) {
            instrumentSystem.remove_inst(cursorPosition.y);
            orders.removeTable(cursorPosition.y);
            indexes.removeInst(cursorPosition.y);
            if (orders.tableCount() == 0)
              patternMenu_orderIndex = 0;
            global_unsavedChanges = true;
          } else if (global_currentMenu == GlobalMenus::order_menu &&
                     indexes.rowCount() > 1) {
            indexes.removeRow(cursorPosition.y);
            global_unsavedChanges = true;
          } else if (global_currentMenu == GlobalMenus::order_management_menu &&
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
            global_unsavedChanges = true;
          }
          break;
        }
        case 'c': {
          if (global_currentMenu == GlobalMenus::instrument_menu &&
              instrumentSystem.inst_count() > 0) {
            instrumentSystem.at(cursorPosition.y)->cycle_type();
            global_unsavedChanges = true;
          } else if (global_currentMenu == GlobalMenus::order_management_menu &&
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
            order *input = orders.at(cursorPosition.selection.y)
                               ->at(cursorPosition.selection.x);
            for (int i = 0; i < patternLength; i++) {
              row *inputRow = input->at(i);
              row *outputRow = output->at(i);
              outputRow->feature = inputRow->feature;
              outputRow->note = inputRow->note;
              outputRow->octave = inputRow->octave;
              outputRow->volume = inputRow->volume;
              for (int i = 0;
                   i < 4 && static_cast<size_t>(i) < inputRow->effects.size() &&
                   static_cast<size_t>(i) < outputRow->effects.size();
                   i++) {
                outputRow->effects.at(i).type = inputRow->effects.at(i).type;
                outputRow->effects.at(i).effect =
                    inputRow->effects.at(i).effect;
              }
            }
            cursorPosition.selection.x = 0;
            cursorPosition.selection.y = 0;
            cursorPosition.subMenu = 0;
            cursorPosition.y = 0;
            cursorPosition.x = 0;
            global_unsavedChanges = true;
          }
          break;
        }
        case 'w': {
          if (global_currentMenu == GlobalMenus::order_menu &&
              instrumentSystem.inst_count() > 0) {
            if (indexes.at(cursorPosition.y)->at(cursorPosition.x) < 254) {
              indexes.at(cursorPosition.y)->increment(cursorPosition.x);
              global_unsavedChanges = true;
            }

            while (indexes.at(cursorPosition.y)->at(cursorPosition.x) >=
                   orders.at(cursorPosition.x)->order_count())
              orders.at(cursorPosition.x)->add_order();
          } else if (global_currentMenu == GlobalMenus::options_menu) {
            if (cursorPosition.y == 0) {
              // RPM
              if ((currentKeyStates[SDL_SCANCODE_LSHIFT] ||
                   currentKeyStates[SDL_SCANCODE_RSHIFT]) &&
                  audio_tenpo < 65525) {
                audio_tenpo += 10;
                global_unsavedChanges = true;
              } else if (audio_tenpo < 65534) {
                audio_tenpo++;
                global_unsavedChanges = true;
              }
            } else {
              if (patternLength < 256) {
                patternLength++;
                orders.setRowCount(patternLength);
                global_unsavedChanges = true;
              }
            }
          }
          break;
        }
        case 's': {
          if (global_currentMenu == GlobalMenus::order_menu &&
              instrumentSystem.inst_count() > 0) {
            if (indexes.at(cursorPosition.y)->at(cursorPosition.x) > 0) {
              indexes.at(cursorPosition.y)->decrement(cursorPosition.x);
              global_unsavedChanges = true;
            }
          } else if (global_currentMenu == GlobalMenus::options_menu) {
            if (cursorPosition.y == 0) {
              // RPM
              if ((currentKeyStates[SDL_SCANCODE_LSHIFT] ||
                   currentKeyStates[SDL_SCANCODE_RSHIFT]) &&
                  audio_tenpo > 40) {
                audio_tenpo -= 10;
                global_unsavedChanges = true;
              } else if (audio_tenpo > 30) {
                audio_tenpo--;
                global_unsavedChanges = true;
              }
            } else {
              if (patternLength > 16) {
                patternLength--;
                orders.setRowCount(patternLength);
                global_unsavedChanges = true;
              }
            }
          }
          break;
        }
        }
      }
      return;
    }
    return;
}