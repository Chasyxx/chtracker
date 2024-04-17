/*
  Copyright (C) 2024 Chase Taylor

  This file is part of chTRACKER at path ./src/chtracker.cxx

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

#include <SDL2/SDL.h>

#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_video.h>
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <strings.h>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

extern "C" {
#include "visual.h"
}

#include "channel.hxx"
#include "order.hxx"
#include "timer.hxx"

#define TILE_SIZE 96
#define TILE_SIZE_F 96.0

// Quit SDL and terminate with code.
void quit(int code = 0) {
  SDL_Quit();
  exit(code);
}

unsigned long video_frame;
int lastWindowWidth, lastWindowHeight;

unsigned short rowsPerMinute = 960;
unsigned short rowCount = 32;

constexpr unsigned char global_majorVersion = 0x00;
constexpr unsigned char global_minorVersion = 0x01;
constexpr unsigned char global_patchVersion = 0x00;
constexpr unsigned char global_prereleaseVersion = 0x01;

enum state {
  main_menu,

  help_menu,
  order_menu,
  pattern_menu,
  instrument_menu,
  order_management_menu,
  options_menu,
  file_menu,
  save_file_menu
};

struct selection {
  unsigned int x = 0;
  unsigned int y = 0;
};

struct cursorPos {
  unsigned int x = 0;
  unsigned int y = 0;
  unsigned char subMenu = 0;
  struct selection selection;
};

cursorPos cursorPosition;

void onOpenMenu() { cursorPosition = {0, 0, 0, {0, 0}}; }

state global_state = main_menu;
unsigned long audio_time = 0;
bool global_playing = false;
orderStorage orders(32);
instrumentStorage instruments;
orderIndexStorage indexes;
timerHandler timers;

unsigned char audio_row = -1;
unsigned short audio_pattern = 0;

unsigned short patternMenu_orderIndex = 0;
char patternMenu_viewMode = 3;
constexpr unsigned char patternMenu_instrumentCollumnWidth[] = {3,  6,  12,
                                                                18, 24, 30};
constexpr unsigned char patternMenu_instrumentVariableCount[] = {2,  4,  9,
                                                                 14, 19, 24};

#if defined(_WIN32)
std::string fileMenu_directoryPath = "C:\\";
#define PATH_SEPERATOR_S "\\"
#define PATH_SEPERATOR '\\'
#elif defined(__linux__) || defined(__APPLE__)
std::string fileMenu_directoryPath = "/";
#define PATH_SEPERATOR_S "/"
#define PATH_SEPERATOR '/'
#define _POSIX
#else
std::string fileMenu_directoryPath = "/";
#define PATH_SEPERATOR_S "/"
#define PATH_SEPERATOR '/'
#define HOPEFULLY_POSIX
#endif

char *fileMenu_errorText = const_cast<char *>("");
std::string saveFileMenu_fileName = "file.cht";

int saveFile(std::filesystem::path path) {
  std::ofstream file(path, std::ios::out | std::ios::binary);
  if (!file || !file.is_open())
    return 1;
  unsigned char *buffer = new unsigned char[256];
  // Version
  for (int i = 0; i < 9; i++) {
    buffer[i] = "CHTRACKER"[i];
  }
  buffer[9] = global_majorVersion;
  buffer[10] = global_minorVersion;
  buffer[11] = global_patchVersion;
  buffer[12] = global_prereleaseVersion;

  buffer[13] = static_cast<unsigned char>(rowsPerMinute >> 8);
  buffer[14] = static_cast<unsigned char>(rowsPerMinute & 255);

  buffer[15] = static_cast<unsigned char>(rowCount >> 8);
  buffer[16] = static_cast<unsigned char>(rowCount & 255);

  {
    unsigned short orderCount = indexes.rowCount();
    buffer[17] = static_cast<unsigned char>(orderCount >> 8);
    buffer[18] = static_cast<unsigned char>(orderCount & 255);
  }
  buffer[19] = instruments.inst_count();

  // Fill unused space with FF
  for (int i = 20; i < 256; i++) {
    buffer[i] = 0xff;
  }
  file.write(reinterpret_cast<const char *>(buffer), 256);
  delete[] buffer;
  for (int i = 0; i < instruments.inst_count(); i++) {
    buffer = new unsigned char[8];
    buffer[0] = static_cast<unsigned char>(instruments.at(i)->get_type());
    buffer[1] = orders.at(i)->order_count();
    buffer[2] = 0xff;
    buffer[3] = 0xff;
    buffer[4] = 0xff;
    buffer[5] = 0xff;
    buffer[6] = 0xff;
    buffer[7] = 0xff;
    file.write(reinterpret_cast<const char *>(buffer), 8);
    delete[] buffer;
  }
  {
    for (unsigned short orderRowIndex = 0; orderRowIndex < indexes.rowCount();
         orderRowIndex++) {
      buffer = new unsigned char[orders.tableCount()];
      try {
        for (unsigned char orderIndex = 0;
             orderIndex < indexes.at(orderRowIndex)->instCount();
             orderIndex++) {
          try {
            buffer[orderIndex] = indexes.at(orderRowIndex)->at(orderIndex);
          } catch (std::out_of_range &) {
            buffer[orderIndex] = 0;
          }
        }
      } catch (std::out_of_range &) {
        continue;
      }
      try {
        file.write(reinterpret_cast<const char *>(buffer),
                   indexes.at(orderRowIndex)->instCount());
      } catch (std::out_of_range &) {
        file.write(reinterpret_cast<const char *>(buffer), orders.tableCount());
      }
      delete[] buffer;
    }
    for (unsigned char tableIdx = 0; tableIdx < orders.tableCount();
         tableIdx++) {
      instrumentOrderTable *table = orders.at(tableIdx);
      for (unsigned char orderIdx = 0; orderIdx < table->order_count();
           orderIdx++) {
        order *o = table->at(orderIdx);
        for (unsigned short rowIdx = 0; rowIdx < o->rowCount(); rowIdx++) {
          row *r = o->at(rowIdx);
          buffer = new unsigned char[32];
          switch (r->feature) {
          case rowFeature::empty:
            buffer[0] = 0;
            break;
          case rowFeature::note:
            buffer[0] = 1;
            break;
          case rowFeature::note_cut:
            buffer[0] = 2;
            break;
          }
          buffer[1] = ((r->note - 'A') & 15) << 4;
          buffer[1] |= r->octave & 15;
          buffer[2] = r->volume;
          for (size_t i = 0; i < r->effects.size() && i < 4; i++) {
            const effect &e = r->effects.at(i);
            buffer[3 + (i * 3)] = static_cast<unsigned char>(e.type);
            buffer[4 + (i * 3)] = e.effect >> 8 & 255;
            buffer[5 + (i * 3)] = e.effect & 255;
          }
          for (int i = 15; i < 32; i++) {
            buffer[i] = 0xff;
          }
          file.write(reinterpret_cast<const char *>(buffer), 32);
          delete[] buffer;
        }
      }
    }
  }
  file.close();
  return 0;
}

int loadFile(std::filesystem::path filePath) {
  global_playing = false;
  while (timers.hasTimer("row"))
    ;
  std::ifstream file(filePath, std::ios::in | std::ios::binary);
  if (!file || !file.is_open())
    return 1;
  unsigned char *buffer = new unsigned char[256];
  file.read(reinterpret_cast<char *>(buffer), 256);
  for (int i = 0; i < 9; i++) {
    if (buffer[i] != "CHTRACKER"[i]) {
      fileMenu_errorText = const_cast<char*>("Not a chTRACKER file");
      delete[] buffer;
      file.close();
      return 1;
    }
  }
  if (buffer[9] != global_majorVersion) {
    fileMenu_errorText = const_cast<char*>("Major version mismatch");
    delete[] buffer;
    file.close();
    return 1;
  }
  if (buffer[10] > global_minorVersion) {
    fileMenu_errorText = const_cast<char*>("Newer minor version");
    delete[] buffer;
    file.close();
    return 1;
  }
  unsigned short local_rowsPerMinute = static_cast<unsigned short>(buffer[13])
                                           << 8 |
                                       static_cast<unsigned short>(buffer[14]);
  unsigned short local_rowsPerPattern = static_cast<unsigned short>(buffer[15])
                                            << 8 |
                                        static_cast<unsigned short>(buffer[16]);
  unsigned short local_orderCount = static_cast<unsigned short>(buffer[17])
                                        << 8 |
                                    static_cast<unsigned short>(buffer[18]);
  unsigned char local_instrumentCount = buffer[19];
  delete[] buffer;

  std::vector<unsigned char> instrumentOrderCounts(0);

  buffer = new unsigned char[local_instrumentCount * 8];
  file.read(reinterpret_cast<char *>(buffer), local_instrumentCount * 8);
  while (instruments.inst_count() > 0)
    instruments.remove_inst(instruments.inst_count() - 1);
  for (unsigned char instrumentIdx = 0; instrumentIdx < local_instrumentCount;
       instrumentIdx++) {
    unsigned char *i = buffer + (instrumentIdx * 8);
    instruments.add_inst(static_cast<audioChannelType>(i[0]));
    instrumentOrderCounts.push_back(i[1]);
  }
  delete[] buffer;

  buffer = new unsigned char[local_instrumentCount * local_orderCount];
  file.read(reinterpret_cast<char *>(buffer),
            local_instrumentCount * local_orderCount);
  while (indexes.rowCount() > 0)
    indexes.removeRow(indexes.rowCount() - 1);
  for (unsigned short orderIdx = 0; orderIdx < local_orderCount; orderIdx++) {
    orderIndexRow *r = indexes.at(indexes.addRow());
    while (r->instCount() > 0)
      r->removeInst(r->instCount() - 1);
    unsigned char *i = buffer + (orderIdx * local_instrumentCount);
    for (unsigned char instrumentIdx = 0; instrumentIdx < local_instrumentCount;
         instrumentIdx++) {
      r->set(r->addInst(), i[instrumentIdx]);
    }
  }
  delete[] buffer;

  orders.setRowCount(local_rowsPerPattern);
  while (orders.tableCount() > 0)
    orders.removeTable(orders.tableCount() - 1);
  for (unsigned char tableIdx = 0; tableIdx < local_instrumentCount;
       tableIdx++) {
    buffer = new unsigned char[instrumentOrderCounts.at(tableIdx) *
                               local_rowsPerPattern * 32];
    file.read(reinterpret_cast<char *>(buffer),
              instrumentOrderCounts.at(tableIdx) * local_rowsPerPattern * 32);
    instrumentOrderTable *table = orders.at(orders.addTable());
    table->set_row_count(local_rowsPerPattern);
    for (unsigned char orderIdx = 0;
         orderIdx < instrumentOrderCounts.at(tableIdx); orderIdx++) {
      unsigned char *oFile = buffer + (orderIdx * local_rowsPerPattern * 32);
      order *oInternal = table->at(table->add_order());
      oInternal->setRowCount(local_rowsPerPattern);
      for (unsigned short rowIdx = 0; rowIdx < local_rowsPerPattern; rowIdx++) {
        unsigned char *rFile = oFile + (rowIdx * 32);
        row *rInternal = oInternal->at(rowIdx);
        //     switch(r->feature) {
        //         case rowFeature::empty:     buffer[0] = 0; break;
        //         case rowFeature::note:      buffer[0] = 1; break;
        //         case rowFeature::note_cut:  buffer[0] = 2; break;
        //     }
        switch (rFile[0]) {
        case 0:
        default:
          rInternal->feature = rowFeature::empty;
          break;
        case 1:
          rInternal->feature = rowFeature::note;
          break;
        case 2:
          rInternal->feature = rowFeature::note_cut;
          break;
        }
        rInternal->note = (rFile[1] >> 4 & 15) + 'A';
        rInternal->octave = rFile[1] & 15;
        rInternal->volume = rFile[2];
        rInternal->effects = std::vector<effect>(4);
        for (unsigned char effectIdx = 0; effectIdx < 4; effectIdx++) {
          effect &eInternal = rInternal->effects.at(effectIdx);
          unsigned char *eFile = rFile + 3 + (effectIdx * 3);
          eInternal.type = static_cast<effectTypes>(eFile[0]);
          eInternal.effect =
              ((static_cast<unsigned short>(eFile[1] & 255) << 8) +
               (static_cast<unsigned short>(eFile[2] & 255)));
        }
      }
    }
    delete[] buffer;
  }

  rowsPerMinute = local_rowsPerMinute;
  rowCount = local_rowsPerPattern;
  file.close();
  return 0;
}

void audioCallback(void *userdata, Uint8 *stream, int len) {
  (void)userdata;
  if (global_state == main_menu) {
    for (unsigned int i = 0; i < static_cast<unsigned int>(len); i++) {
      unsigned long t = audio_time + i;
      stream[i] = (t * 3 / 2 & t >> 8) | (t * 5 / 4 & t >> 11);
    }
    audio_time += len;
    return;
  }

  if (global_playing && orders.tableCount() > 0) {
    if (!timers.hasTimer("row"))
      timers.addTimer("row", 48000 * 60 / rowsPerMinute);
    if (!timers.hasTimer("effect"))
      timers.addTimer("effect", 375 * 960 / rowsPerMinute);
    if (!timers.hasTimer("arpeggio"))
      timers.addTimer("arpeggio", 1500 * 960 / rowsPerMinute);
    for (unsigned int audioIdx = 0; audioIdx < static_cast<unsigned int>(len);
         audioIdx++) {
      unsigned char sample = 0;
      for (unsigned char instrumentIdx = 0;
           instrumentIdx < instruments.inst_count(); instrumentIdx++) {
        sample = static_cast<unsigned char>(
            std::min(255, static_cast<unsigned short>(sample) +
                              static_cast<unsigned short>(
                                  instruments.at(instrumentIdx)->gen() / 4)));
      }
      stream[audioIdx] = sample;
      timers.tick();
      if (timers.isComplete("row")) {
        audio_row++;
        if (audio_row >= orders.at(0)->at(0)->rowCount()) {
          audio_row = 0;
          audio_pattern++;
          if (audio_pattern >= indexes.rowCount()) {
            audio_pattern = 0;
          }
        }
        timers.resetTimer("row", 48000 * 60 / rowsPerMinute);
        for (unsigned char i = 0; i < instruments.inst_count(); i++) {
          unsigned char patternIndex = indexes.at(audio_pattern)->at(i);
          row *currentRow = orders.at(i)->at(patternIndex)->at(audio_row);
          instruments.at(i)->set_row(*currentRow);
        }
      }
    }
    if (timers.isComplete("effect")) {
      for (unsigned char i = 0; i < instruments.inst_count(); i++) {
        instruments.at(i)->applyFx();
      }
      timers.resetTimer("effect", 0);
    }
    if (timers.isComplete("arpeggio")) {
      for (unsigned char i = 0; i < instruments.inst_count(); i++) {
        instruments.at(i)->applyArpeggio();
      }
      timers.resetTimer("arpeggio", 0);
    }
  } else {
    if (timers.hasTimer("row"))
      timers.removeTimer("row");
    if (timers.hasTimer("effect"))
      timers.removeTimer("effect");
    if (timers.hasTimer("arpeggio"))
      timers.removeTimer("arpeggio");
    audio_row = -1;
    audio_pattern = patternMenu_orderIndex;
    row dummyRow = {rowFeature::note_cut, 'A', 4, 0, std::vector<effect>(0)};
    for (char i = 0; i < 4; i++)
      dummyRow.effects.push_back(effect{effectTypes::arpeggio, 0});
    for (unsigned char i = 0; i < instruments.inst_count(); i++) {
      instruments.at(i)->set_row(dummyRow);
    }
    for (unsigned int i = 0; i < static_cast<unsigned int>(len); i++) {
      stream[i] =
          static_cast<Uint8>((static_cast<short>(stream[i]) - 128) / 2 + 128);
    }
  }
  audio_time += len;
}

void init(/*SDL_Renderer *renderer, */ SDL_Window *window) {
  int windowWidth, windowHeight;
  SDL_GetWindowSize(window, &windowWidth, &windowHeight);

#ifdef _POSIX
  char *str = std::getenv("HOME");
  if (str == nullptr) {
    str = std::getenv("USER");
    if (str == nullptr) {
      fileMenu_directoryPath = "/";
    } else {
      fileMenu_directoryPath = std::string("/home/") + str + "/";
    }
  } else {
    fileMenu_directoryPath = str;
  }
#elif !defined(_WIN32)
  fileMenu_directoryPath = "/";
#endif

  SDL_AudioSpec audioSpec;

  audioSpec.freq = 48000;
  audioSpec.format = AUDIO_U8;
  audioSpec.channels = 1;
  audioSpec.samples = 256;
  audioSpec.callback = audioCallback;

  if (SDL_OpenAudio(&audioSpec, NULL) < 0) {
    std::cerr << "Couldn't open audio! " << SDL_GetError() << std::endl;
    quit(1);
  }
  indexes.addRow();
  SDL_PauseAudio(0);
}

void setLimits(unsigned int &limitX, unsigned int &limitY) {
  switch (global_state) {
  case instrument_menu: {
    limitX = 0;
    limitY = instruments.inst_count() - 1;
    break;
  }
  case order_menu: {
    limitX = indexes.instCount(0) - 1;
    limitY = indexes.rowCount() - 1;
    break;
  }
  case order_management_menu: {
    if (cursorPosition.subMenu == 1 && orders.tableCount() > 0) {
      limitX = orders.tableCount() - 1;
      limitY = 0;
      break;
    }
    limitY = orders.tableCount() - 1;
    if (orders.tableCount() > 0)
      limitX = orders.at(cursorPosition.y)->order_count() - 1;
    else
      limitX = 0;
    break;
  }
  case pattern_menu: {
    if (orders.tableCount() < 1) {
      limitX = limitY = 0;
      break;
    }
    limitX = patternMenu_instrumentVariableCount[static_cast<size_t>(
                 patternMenu_viewMode)] *
                 orders.tableCount() -
             1;
    limitY = orders.at(0)->at(0)->rowCount() - 1;
    break;
  }
  case options_menu: {
    limitX = 0;
    limitY = 1;
    break;
  }
  default:
    break;
  }
}

void sdlEventHandler(SDL_Event *event, int &quit) {
  unsigned int limitX = INT_MAX;
  unsigned int limitY = INT_MAX;
  setLimits(limitX, limitY);
  if (cursorPosition.x > limitX)
    cursorPosition.x = limitX;
  if (cursorPosition.y > limitY)
    cursorPosition.y = limitY;
  const Uint8 *currentKeyStates = SDL_GetKeyboardState(NULL);
  switch (event->type) {
  case SDL_QUIT:
    quit = 1;
    break;
  case SDL_TEXTINPUT: {
    if (global_state != save_file_menu)
      break;
    saveFileMenu_fileName += event->text.text;
    break;
  }
  case SDL_KEYDOWN: {
    SDL_Keysym ks = event->key.keysym;
    SDL_Keycode code = ks.sym;
    if (global_state == save_file_menu) {
      if (code == SDLK_ESCAPE) {
        global_state = file_menu;
        onOpenMenu();
      }
      if (code == SDLK_BACKSPACE && !saveFileMenu_fileName.empty())
        saveFileMenu_fileName.pop_back();
      if (code == SDLK_RETURN || code == SDLK_RETURN2) {
        std::filesystem::path path(fileMenu_directoryPath + PATH_SEPERATOR_S +
                                   saveFileMenu_fileName);
        try {
          if (std::filesystem::exists(path) && cursorPosition.subMenu == 0) {
            cursorPosition.subMenu = 1;
            break;
          }
          int exit_code = saveFile(path);
          if (exit_code) {
            fileMenu_errorText = const_cast<char *>("Refused to save the file");
            global_state = file_menu;
            onOpenMenu();
          } else {
            global_state = pattern_menu;
            onOpenMenu();
          };
        } catch (std::filesystem::filesystem_error &) {
          fileMenu_errorText =
              const_cast<char *>("Filesystem error trying to save the file");
          global_state = file_menu;
          onOpenMenu();
        }
      }
      break;
    }
    if (global_state == main_menu) {
      switch (code) {
      case SDLK_ESCAPE: {
        quit = 1;
        break;
      }
      case 'z': {
        global_state = help_menu;
        break;
      }
      }
    } else {
      switch (code) {
      case SDLK_ESCAPE: {
        if (global_state == file_menu) {
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
        global_state = help_menu;
        onOpenMenu();
        break;
      }
      case SDLK_F2: {
        global_state = order_menu;
        onOpenMenu();
        break;
      }
      case SDLK_F3: {
        global_state = pattern_menu;
        onOpenMenu();
        break;
      }
      case SDLK_F4: {
        global_state = instrument_menu;
        onOpenMenu();
        break;
      }
      case SDLK_F5: {
        global_state = order_management_menu;
        onOpenMenu();
        break;
      }
      case SDLK_F6: {
        global_state = options_menu;
        onOpenMenu();
        break;
      }
      case SDLK_F7: {
        global_state = file_menu;
        onOpenMenu();
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
        if (global_state == file_menu) {
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
                    if (loadFile(entry.path())) {
                      if(SDL_strlen(fileMenu_errorText) == 0)
                        fileMenu_errorText =
                          const_cast<char *>("Refused to load the file");
                    } else {
                      patternMenu_orderIndex = 0;
                      patternMenu_viewMode = 3;
                      global_state = pattern_menu;
                      onOpenMenu();
                    };
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
        global_playing = !global_playing;
        break;
      }
      }
      if (global_state == pattern_menu) {
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
          break;
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
          case 'a':
            r->feature = rowFeature::note;
            r->note = 'A';
            break;
          case 'b':
            r->feature = rowFeature::note;
            r->note = 'B';
            break;
          case 'c':
            r->feature = rowFeature::note;
            r->note = 'C';
            break;
          case 'd':
            r->feature = rowFeature::note;
            r->note = 'D';
            break;
          case 'e':
            r->feature = rowFeature::note;
            r->note = 'E';
            break;
          case 'f':
            r->feature = rowFeature::note;
            r->note = 'F';
            break;
          case 'g':
            r->feature = rowFeature::note;
            r->note = 'G';
            break;
          case 'h':
            r->feature = rowFeature::note;
            r->note = 'H';
            break;
          case 'i':
            r->feature = rowFeature::note;
            r->note = 'I';
            break;
          case 'j':
            r->feature = rowFeature::note;
            r->note = 'J';
            break;
          case 'k':
            r->feature = rowFeature::note;
            r->note = 'K';
            break;
          case 'l':
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
          if (moveDown)
            cursorPosition.y++;
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
          if (moveDown)
            cursorPosition.y++;
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
            if (moveDown)
              cursorPosition.y++;
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
            if (effectIdx == 4) {
              cursorPosition.y++;
              cursorPosition.x -= 4;
            } else
              cursorPosition.x++;
          }
        }
      } else {
        switch (code) {
        case 'z': {
          if (global_state == instrument_menu &&
              instruments.inst_count() < 254) {
            instruments.add_inst(audioChannelType::null);
            orders.at(orders.addTable())->add_order();
            indexes.addInst();
          } else if (global_state == order_menu) {
            indexes.addRow();
          }
          break;
        }
        case 'x': {
          if (global_state == instrument_menu && instruments.inst_count() > 0) {
            instruments.remove_inst(cursorPosition.y);
            orders.removeTable(cursorPosition.y);
            indexes.removeInst(cursorPosition.y);
            if (orders.tableCount() == 0)
              patternMenu_orderIndex = 0;
          } else if (global_state == order_menu && indexes.rowCount() > 1) {
            indexes.removeRow(cursorPosition.y);
          } else if (global_state == order_management_menu &&
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
          }
          break;
        }
        case 'c': {
          if (global_state == instrument_menu && instruments.inst_count() > 0) {
            instruments.at(cursorPosition.y)->cycle_type();
          } else if (global_state == order_management_menu &&
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
            for (int i = 0; i < rowCount; i++) {
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
          }
          break;
        }
        case 'w': {
          if (global_state == order_menu && instruments.inst_count() > 0) {
            if (indexes.at(cursorPosition.y)->at(cursorPosition.x) < 254)
              indexes.at(cursorPosition.y)->increment(cursorPosition.x);

            while (indexes.at(cursorPosition.y)->at(cursorPosition.x) >=
                   orders.at(cursorPosition.x)->order_count())
              orders.at(cursorPosition.x)->add_order();
          } else if (global_state == options_menu) {
            if (cursorPosition.y == 0) {
              // RPM
              if ((currentKeyStates[SDL_SCANCODE_LSHIFT] ||
                   currentKeyStates[SDL_SCANCODE_RSHIFT]) &&
                  rowsPerMinute < 65525) {
                rowsPerMinute += 10;
              } else if (rowsPerMinute < 65534) {
                rowsPerMinute++;
              }
            } else {
              if (rowCount < 256) {
                rowCount++;
                orders.setRowCount(rowCount);
              }
            }
          }
          break;
        }
        case 's': {
          if (global_state == order_menu && instruments.inst_count() > 0) {
            if (indexes.at(cursorPosition.y)->at(cursorPosition.x) > 0)
              indexes.at(cursorPosition.y)->decrement(cursorPosition.x);
          } else if (global_state == options_menu) {
            if (cursorPosition.y == 0) {
              // RPM
              if ((currentKeyStates[SDL_SCANCODE_LSHIFT] ||
                   currentKeyStates[SDL_SCANCODE_RSHIFT]) &&
                  rowsPerMinute > 40) {
                rowsPerMinute -= 10;
              } else if (rowsPerMinute > 30) {
                rowsPerMinute--;
              }
            } else {
              if (rowCount > 16) {
                rowCount--;
                orders.setRowCount(rowCount);
              }
            }
          } else if (global_state == file_menu) {
            global_state = save_file_menu;
          }
          break;
        }
        }
      }
      break;
    }
  }
  }
  setLimits(limitX, limitY);
  if (cursorPosition.x > limitX)
    cursorPosition.x = limitX;
  if (cursorPosition.y > limitY)
    cursorPosition.y = limitY;
}

char *getTypeName(audioChannelType type, bool short_name) {
  char *str = const_cast<char *>("Error");
  switch (type) {
  case audioChannelType::null:
    if (short_name) {
      str = const_cast<char *>("--");
      break;
    };
    str = const_cast<char *>("Silent");
    break;
  case audioChannelType::lfsr8:
    if (short_name) {
      str = const_cast<char *>("NS");
      break;
    };
    str = const_cast<char *>("8 bit LFSR");
    break;
  case audioChannelType::lfsr14:
    if (short_name) {
      str = const_cast<char *>("NL");
      break;
    };
    str = const_cast<char *>("14bit LFSR");
    break;
  case audioChannelType::pulse:
    if (short_name) {
      str = const_cast<char *>("SQ");
      break;
    };
    str = const_cast<char *>("Square");
    break;
  case audioChannelType::triangle:
    if (short_name) {
      str = const_cast<char *>("TR");
      break;
    };
    str = const_cast<char *>("Triangle");
    break;
  case audioChannelType::sawtooth:
    if (short_name) {
      str = const_cast<char *>("SW");
      break;
    };
    str = const_cast<char *>("Sawtooth");
    break;
  }
  return str;
}

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

void screenUpdate(SDL_Renderer *renderer, SDL_Window *window) {
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

  // Background

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

  // *not* background
  int i = 0;
  const char *str = "chTRACKER";
  if (global_state == main_menu) {
    while (str[i] != 0) {
      text_drawBigChar(
          renderer, indexes_charToIdx(str[i]), 4,
          windowWidth / 2 - 144 + (i * 32),
          static_cast<int>(windowHeight / 3.0 +
                           SDL_sin(i / 3.0 + (millis / 500.0)) * 16),
          visual_whiteText, 0);
      i++;
    }
    text_drawText(renderer, const_cast<char *>("Press Z"), 3,
                  windowWidth / 2 - 96, windowHeight * 2 / 3, visual_whiteText,
                  0, 10);
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
      if(global_prereleaseVersion > 0) {
        visual_numberToString(str, global_prereleaseVersion);
        versionString += "-";
        versionString += str;
      }
      text_drawText(renderer, const_cast<char *>(versionString.c_str()), 2,
                    0, windowHeight-16, visual_whiteText,
                    0, fontTileCountW);
      text_drawText(renderer, const_cast<char *>("NO WARRANTY for his program! The copyright holders will not be held liable for damages arising from this program.\nCopyright \xcc 2024 Chase Taylor. Licensed under GPL."), 1,
                    0, 0, visual_whiteText,
                    0, fontTileCountW*2);
    }
  } else {
    short xOffset = 0;
    text_drawText(renderer, const_cast<char *>("Help!"), 1, xOffset, 0,
                  visual_whiteText, global_state == help_menu, windowWidth / 8);
    xOffset += 6 * 8;
    text_drawText(renderer, const_cast<char *>("Order"), 1, xOffset, 0,
                  visual_whiteText, global_state == order_menu,
                  windowWidth / 8);
    xOffset += 6 * 8;
    text_drawText(renderer, const_cast<char *>("Pat."), 1, xOffset, 0,
                  visual_whiteText, global_state == pattern_menu,
                  windowWidth / 8);
    xOffset += 5 * 8;
    text_drawText(renderer, const_cast<char *>("Inst."), 1, xOffset, 0,
                  visual_whiteText, global_state == instrument_menu,
                  windowWidth / 8);
    xOffset += 6 * 8;
    text_drawText(renderer, const_cast<char *>("OrdMan."), 1, xOffset, 0,
                  visual_whiteText, global_state == order_management_menu,
                  windowWidth / 8);
    xOffset += 8 * 8;
    text_drawText(renderer, const_cast<char *>("Options"), 1, xOffset, 0,
                  visual_whiteText, global_state == options_menu,
                  windowWidth / 8);
    xOffset += 8 * 8;
    text_drawText(renderer, const_cast<char *>("File"), 1, xOffset, 0,
                  visual_whiteText, global_state == file_menu, windowWidth / 8);
    // l+=6;
    // text_drawText(renderer, const_cast<char *>("Order"), 1, l*8, 0,
    // visual_whiteText, global_state==preset_menu, windowWidth/8);
    SDL_SetRenderDrawColor(renderer, 63, 127, 255, 255);
    SDL_Rect borderRectangle = {0, 8, windowWidth, 8};
    SDL_RenderFillRect(renderer, &borderRectangle);
    switch (global_state) {
    case main_menu: {
      text_drawText(renderer, const_cast<char *>("Error"), 3, 18, 18,
                    visual_redText, 1, 10);
      break;
    }
    case help_menu: {
      // text_drawText(renderer, const_cast<char *>("Insert help here"), 2, 0,
      // 16, visual_whiteText, 1, fontTileCountW);
      std::ifstream helpFile("./doc/help.txt", std::ios::in);
#ifdef _POSIX
      if (!helpFile.is_open()) {
        helpFile.open("/usr/share/doc/chtracker/help.txt");
      }
#endif
      if (!helpFile.is_open()) {
        text_drawText(renderer, const_cast<char *>("Couldn't open help"), 2, 0,
                      16, visual_redText, 1, 19);
        break;
      }
      helpFile.seekg(0);
      if (helpFile.fail()) {
        text_drawText(renderer, const_cast<char *>("Couldn't read help"), 2, 0,
                      16, visual_redText, 1, 19);
        break;
      }
      unsigned int undrawnLines = cursorPosition.y;
      unsigned int drawnRow = 0;
      char buf[80];
      while (!helpFile.eof() && !helpFile.fail()) {
        for (unsigned short i = 0; i < 80; i++) {
          buf[i] = 0;
        }
        helpFile.getline(buf, 80);
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
      break;
    }
    case order_menu: {
      text_drawText(renderer, const_cast<char *>("Order rows: "), 2, 0, 16,
                    visual_whiteText, 0, fontTileCountW);
      char *numberStr = new char[4];
      visual_numberToString(numberStr, indexes.rowCount());
      text_drawText(renderer, numberStr, 2, 13 * 16, 16, visual_whiteText, 0,
                    fontTileCountW);
      text_drawText(renderer, const_cast<char *>("Z to add row"), 2, 0, 32,
                    visual_whiteText, 0, fontTileCountW);
      text_drawText(renderer, const_cast<char *>("X to remove selected row"), 2,
                    0, 48, visual_whiteText, 0, fontTileCountW);
      text_drawText(renderer,
                    const_cast<char *>("W to increment selected order"), 2, 0,
                    64, visual_whiteText, 0, fontTileCountW);
      text_drawText(renderer,
                    const_cast<char *>("S to decrement selected order"), 2, 0,
                    80, visual_whiteText, 0, fontTileCountW);
      unsigned short startingRow = static_cast<unsigned short>(
          std::max(0, static_cast<int>(cursorPosition.y) -
                          static_cast<int>(fontTileCountH / 2)));
      unsigned short currentRow = 0;
      for (unsigned short i = startingRow; i < indexes.rowCount(); i++) {
        unsigned short y = 16 * (static_cast<unsigned short>(currentRow) + 8);
        if (y >= windowHeight)
          break;
        char letters[4];
        hex4(i, letters);
        for (unsigned char hexNumberIndex = 0; hexNumberIndex < 4;
             hexNumberIndex++)
          text_drawBigChar(renderer, indexes_charToIdx(letters[hexNumberIndex]),
                           2, hexNumberIndex * 16, y, visual_whiteText,
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
          char letter1, letter2;

          hex2(orderIndex, letter1, letter2);

          text_drawBigChar(renderer, indexes_charToIdx(letter1), 2, x, y,
                           (i == cursorPosition.y && j == cursorPosition.x) ||
                                   orderIndex != 0
                               ? visual_greenText
                               : SDL_Color{32, 64, 32, 255},
                           i == cursorPosition.y && j == cursorPosition.x);
          text_drawBigChar(renderer, indexes_charToIdx(letter2), 2, x + 16, y,
                           (i == cursorPosition.y && j == cursorPosition.x) ||
                                   orderIndex != 0
                               ? visual_greenText
                               : SDL_Color{32, 64, 32, 255},
                           i == cursorPosition.y && j == cursorPosition.x);

          if (currentRow == 0) {
            hex2(j, letter1, letter2);

            text_drawBigChar(renderer, indexes_charToIdx(letter1), 2, x, y - 16,
                             visual_whiteText, j == cursorPosition.x);
            text_drawBigChar(renderer, indexes_charToIdx(letter2), 2, x + 16,
                             y - 16, visual_whiteText, j == cursorPosition.x);
          }
        }
        currentRow++;
      }
      break;
    }
    case pattern_menu: {
      barrier(renderer, 48, windowWidth);
      if (orders.tableCount() == 0) {
        text_drawText(
            renderer,
            const_cast<char *>("Add instruments in the Inst. tab (F4)"), 2, 0,
            16, visual_whiteText, 0, fontTileCountW);
        break;
      }
      text_drawText(renderer, const_cast<char *>("Order"), 2, 0, 16,
                    visual_whiteText, 0, 5);
      orderIndexRow *orderRow =
          indexes.at(global_playing ? audio_pattern : patternMenu_orderIndex);
      char letters[4];
      int cursorY = global_playing ? audio_row : cursorPosition.y;
      hex4(global_playing ? audio_pattern : patternMenu_orderIndex, letters);
      for (unsigned char hexNumberIndex = 0; hexNumberIndex < 4;
           hexNumberIndex++)
        text_drawBigChar(renderer, indexes_charToIdx(letters[hexNumberIndex]),
                         2, (6 + hexNumberIndex) * 16, 16, visual_whiteText, 0);

      unsigned char selectedInstrument =
          cursorPosition.x /
          patternMenu_instrumentVariableCount[static_cast<size_t>(
              patternMenu_viewMode)];
      unsigned char selectedVariable =
          cursorPosition.x %
          patternMenu_instrumentVariableCount[static_cast<size_t>(
              patternMenu_viewMode)];

      short instrumentScreenCount =
          fontTileCountW /
          patternMenu_instrumentCollumnWidth[static_cast<size_t>(
              patternMenu_viewMode)];
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
        hex2(orderIndex, letters[0], letters[1]);
        text_drawBigChar(renderer, indexes_charToIdx(letters[0]), 2, x, 32,
                         visual_greenText, collumnIndex == selectedInstrument);
        text_drawBigChar(renderer, indexes_charToIdx(letters[1]), 2, x + 16, 32,
                         visual_greenText, collumnIndex == selectedInstrument);
        currentCollumn++;
      };
      currentCollumn = 0;
      for (unsigned char instrumentIndex = firstInstrument;
           instrumentIndex < orders.tableCount(); instrumentIndex++) {
        // if(16*(5+currentCollumn)>=windowWidth) break;
        order *currentOrder =
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

          if (patternMenu_viewMode >= 1 && currentCollumn == 0)
            text_drawBigChar(renderer, indexes_charToIdx('\x1c'), 2, 16 * 3, y,
                             visual_greyText, 0);

          if (currentCollumn == 0) {
            hex2(rowIndex, letters[0], letters[1]);
            for (unsigned char hexNumberIndex = 0; hexNumberIndex < 2;
                 hexNumberIndex++)
              text_drawBigChar(renderer,
                               indexes_charToIdx(letters[hexNumberIndex]), 2,
                               hexNumberIndex * 16, y, visual_greyText,
                               global_playing && cursorY == rowIndex);
          }
          if (currentRow == 0) {
            if (patternMenu_instrumentCollumnWidth[static_cast<size_t>(
                    patternMenu_viewMode)] >= 13) {
              text_drawText(
                  renderer,
                  getTypeName(instruments.at(instrumentIndex)->get_type(),
                              false),
                  2, 16 * (7 + localCurrentCollumn), 64, visual_whiteText,
                  instrumentIndex == selectedInstrument ||
                      (global_playing && cursorY == rowIndex),
                  10);
            } else if (patternMenu_instrumentCollumnWidth[static_cast<size_t>(
                           patternMenu_viewMode)] >= 5) {
              text_drawText(
                  renderer,
                  getTypeName(instruments.at(instrumentIndex)->get_type(),
                              true),
                  2, 16 * (7 + localCurrentCollumn), 64, visual_whiteText,
                  instrumentIndex == selectedInstrument ||
                      (global_playing && cursorY == rowIndex),
                  2);
            }
            hex2(instrumentIndex, letters[0], letters[1]);
            text_drawBigChar(renderer, indexes_charToIdx(letters[0]), 2,
                             16 * (4 + localCurrentCollumn), 64,
                             visual_greenText,
                             instrumentIndex == selectedInstrument ||
                                 (global_playing && cursorY == rowIndex));
            text_drawBigChar(renderer, indexes_charToIdx(letters[1]), 2,
                             16 * (5 + localCurrentCollumn), 64,
                             visual_greenText,
                             instrumentIndex == selectedInstrument ||
                                 (global_playing && cursorY == rowIndex));
          }

          row *r = currentOrder->at(rowIndex);
          switch (r->feature) {
          case rowFeature::note: {
            text_drawBigChar(renderer, indexes_charToIdx(r->note), 2,
                             16 * (4 + localCurrentCollumn), y,
                             visual_whiteText,
                             (rowSeleted && selectedVariable == 0) ||
                                 (global_playing && cursorY == rowIndex));
            text_drawBigChar(renderer, indexes_charToIdx(hex(r->octave)), 2,
                             16 * (5 + localCurrentCollumn), y,
                             visual_whiteText,
                             (rowSeleted && selectedVariable == 1) ||
                                 (global_playing && cursorY == rowIndex));
            localCurrentCollumn += 3;
            if (patternMenu_viewMode >= 1) {
              hex2(r->volume, letters[0], letters[1]);
              text_drawBigChar(renderer, indexes_charToIdx(letters[0]), 2,
                               16 * (4 + localCurrentCollumn), y,
                               visual_greenText,
                               (rowSeleted && selectedVariable == 2) ||
                                   (global_playing && cursorY == rowIndex));
              text_drawBigChar(renderer, indexes_charToIdx(letters[1]), 2,
                               16 * (5 + localCurrentCollumn), y,
                               visual_greenText,
                               (rowSeleted && selectedVariable == 3) ||
                                   (global_playing && cursorY == rowIndex));
              localCurrentCollumn += 3;
            }
            break;
          }
          case rowFeature::empty: {
            text_drawBigChar(renderer, indexes_charToIdx('.'), 2,
                             16 * (4 + localCurrentCollumn), y,
                             visual_whiteText,
                             (rowSeleted && selectedVariable == 0) ||
                                 (global_playing && cursorY == rowIndex));
            text_drawBigChar(renderer, indexes_charToIdx('.'), 2,
                             16 * (5 + localCurrentCollumn), y,
                             visual_whiteText,
                             (rowSeleted && selectedVariable == 1) ||
                                 (global_playing && cursorY == rowIndex));
            localCurrentCollumn += 3;
            if (patternMenu_viewMode >= 1) {
              text_drawBigChar(renderer, indexes_charToIdx('.'), 2,
                               16 * (4 + localCurrentCollumn), y,
                               visual_greenText,
                               (rowSeleted && selectedVariable == 2) ||
                                   (global_playing && cursorY == rowIndex));
              text_drawBigChar(renderer, indexes_charToIdx('.'), 2,
                               16 * (5 + localCurrentCollumn), y,
                               visual_greenText,
                               (rowSeleted && selectedVariable == 3) ||
                                   (global_playing && cursorY == rowIndex));
              localCurrentCollumn += 3;
            }
            break;
          }
          case rowFeature::note_cut: {
            text_drawBigChar(renderer, indexes_charToIdx('='), 2,
                             16 * (4 + localCurrentCollumn), y,
                             visual_whiteText,
                             (rowSeleted && selectedVariable == 0) ||
                                 (global_playing && cursorY == rowIndex));
            text_drawBigChar(renderer, indexes_charToIdx('='), 2,
                             16 * (5 + localCurrentCollumn), y,
                             visual_whiteText,
                             (rowSeleted && selectedVariable == 1) ||
                                 (global_playing && cursorY == rowIndex));
            localCurrentCollumn += 3;
            if (patternMenu_viewMode >= 1) {
              text_drawBigChar(renderer, indexes_charToIdx('.'), 2,
                               16 * (4 + localCurrentCollumn), y,
                               visual_greenText,
                               (rowSeleted && selectedVariable == 2) ||
                                   (global_playing && cursorY == rowIndex));
              text_drawBigChar(renderer, indexes_charToIdx('.'), 2,
                               16 * (5 + localCurrentCollumn), y,
                               visual_greenText,
                               (rowSeleted && selectedVariable == 3) ||
                                   (global_playing && cursorY == rowIndex));
              localCurrentCollumn += 3;
            }
            break;
          }
          }

          for (unsigned char i = 0; i < std::max(patternMenu_viewMode - 1, 0) &&
                                    i < r->effects.size();
               i++) {
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
            text_drawBigChar(renderer, indexes_charToIdx(effect_number), 2,
                             16 * (4 + localCurrentCollumn), y,
                             effect_autoreset ? visual_greenText
                                              : visual_yellowText,
                             (rowSeleted && selectedVariable == 4 + (i * 5)) ||
                                 (global_playing && cursorY == rowIndex));
            hex4(e.effect, letters);
            for (unsigned char hexNumberIndex = 0; hexNumberIndex < 4;
                 hexNumberIndex++)
              text_drawBigChar(
                  renderer, indexes_charToIdx(letters[hexNumberIndex]), 2,
                  16 * (hexNumberIndex + 5 + localCurrentCollumn), y,
                  effect_autoreset ? visual_greenText : visual_yellowText,
                  (rowSeleted &&
                   selectedVariable == 5 + hexNumberIndex + (i * 5)) ||
                      (global_playing && cursorY == rowIndex));
            localCurrentCollumn += 6;
          }
          if (patternMenu_viewMode >= 1)
            text_drawBigChar(renderer, indexes_charToIdx('\x1c'), 2,
                             16 * (3 + localCurrentCollumn), y, visual_greyText,
                             0);
          currentRow++;
        }
        currentCollumn +=
            patternMenu_instrumentCollumnWidth[static_cast<size_t>(
                patternMenu_viewMode)];
      }
      break;
    }
    case instrument_menu: {
      text_drawText(renderer, const_cast<char *>("Instruments: "), 2, 0, 16,
                    visual_whiteText, 0, fontTileCountW);
      unsigned char orderTableCount = orders.tableCount();
      unsigned char instrumentCount = instruments.inst_count();
      unsigned char orderIndexCount =
          indexes.instCount(SDL_max(orderTableCount, instrumentCount));
      unsigned char instCount =
          SDL_max(SDL_max(orderTableCount, instrumentCount), 0);
      if (instCount > 0)
        if (orderTableCount < instCount || instrumentCount < instCount ||
            orderIndexCount < instCount) {
          std::cerr << "Corrected an instrument length issue!" << std::endl;
          while (orders.tableCount() < instCount)
            orders.at(orders.addTable())->add_order();
          while (instruments.inst_count() < instCount)
            instruments.add_inst(audioChannelType::null);
          while (indexes.instCount(instCount) < instCount)
            indexes.addInst();
        }
      char *numberStr = new char[4];
      visual_numberToString(numberStr, instCount);
      text_drawText(renderer, numberStr, 2, 13 * 16, 16, visual_whiteText, 0,
                    fontTileCountW);
      text_drawText(renderer, const_cast<char *>("Z to add instrument"), 2, 0,
                    32, visual_whiteText, 0, fontTileCountW);
      text_drawText(renderer,
                    const_cast<char *>("X to remove selected instrument"), 2, 0,
                    48, visual_whiteText, 0, fontTileCountW);
      text_drawText(renderer, const_cast<char *>("C to change instrument type"),
                    2, 0, 64, visual_whiteText, 0, fontTileCountW);
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
        text_drawText(
            renderer, getTypeName(instruments.at(i)->get_type(), false), 2, 48,
            y, visual_whiteText, i == cursorPosition.y, fontTileCountW);
        currentRow++;
      }
      delete[] numberStr;
      break;
    }
    case order_management_menu: {
      unsigned char startingRow = static_cast<unsigned char>(
          std::max(0, static_cast<short>(cursorPosition.y) -
                          static_cast<short>(fontTileCountH / 4)));
      unsigned char currentRow = 0;
      for (unsigned char i = startingRow; i < orders.tableCount(); i++) {
        unsigned short y = 16 * (static_cast<unsigned short>(currentRow) + 1);
        if (y >= windowHeight / 2)
          break;
        char letter1, letter2;
        hex2(i, letter1, letter2);
        text_drawBigChar(
            renderer, indexes_charToIdx(letter1), 2, 0, y, visual_whiteText,
            i == (cursorPosition.subMenu ? cursorPosition.selection.y
                                         : cursorPosition.y));
        text_drawBigChar(
            renderer, indexes_charToIdx(letter2), 2, 16, y, visual_whiteText,
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
          text_drawBigChar(
              renderer, indexes_charToIdx(letter1), 2, x, y, visual_greenText,
              cursorPosition.subMenu
                  ? (i == cursorPosition.selection.y &&
                     j == cursorPosition.selection.x)
                  : (i == cursorPosition.y && j == cursorPosition.x));
          text_drawBigChar(
              renderer, indexes_charToIdx(letter2), 2, x + 16, y,
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
        text_drawText(renderer,
                      const_cast<char *>("X to delete selected order"), 2, 0,
                      ceiling + 16, visual_whiteText, 0, fontTileCountW);
        text_drawText(renderer, const_cast<char *>("C to clone selected order"),
                      2, 0, ceiling + 32, visual_whiteText, 0, fontTileCountW);
      } else {
        text_drawText(renderer,
                      const_cast<char *>("Choose an instrument to clone to"), 2,
                      0, ceiling + 16, visual_whiteText, 0, fontTileCountW);
        char digits[3];
        digits[2] = 0;
        hex2(cursorPosition.x, digits[0], digits[1]);
        text_drawText(renderer, digits, 2, 0, ceiling + 48, visual_whiteText, 0,
                      fontTileCountW);
        text_drawText(
            renderer,
            getTypeName(instruments.at(cursorPosition.x)->get_type(), false), 2,
            48, ceiling + 48, visual_whiteText, 0, fontTileCountW);
      }
      break;
    }
    case options_menu: {
      text_drawText(renderer, const_cast<char *>("W to increase"), 2, 0, 16,
                    visual_whiteText, 0, fontTileCountW);
      text_drawText(renderer, const_cast<char *>("S to decrease"), 2, 0, 32,
                    visual_whiteText, 0, fontTileCountW);
      text_drawText(renderer, const_cast<char *>("Rows per minute"), 2, 0, 64,
                    visual_whiteText, cursorPosition.y == 0, fontTileCountW);
      text_drawText(renderer, const_cast<char *>("Rows per order"), 2, 0, 80,
                    visual_whiteText, cursorPosition.y == 1, fontTileCountW);
      char numbers[6];
      visual_numberToString(numbers, rowsPerMinute);
      text_drawText(renderer, numbers, 2, 256, 64, visual_whiteText,
                    cursorPosition.y == 0, fontTileCountW);
      visual_numberToString(numbers, rowCount);
      text_drawText(renderer, numbers, 2, 256, 80, visual_whiteText,
                    cursorPosition.y == 1, fontTileCountW);
      break;
    }
    case file_menu: {
      if (fileMenu_errorText[0] == 0) {
        text_drawText(renderer,
                      const_cast<char *>("Welcome to the FILE PICKER"), 2, 0,
                      16, visual_whiteText, 0, fontTileCountW);
      } else {
        text_drawText(renderer, fileMenu_errorText, 2, 0, 16, visual_redText, 0,
                      fontTileCountW);
      }
      barrier(renderer, 32, windowWidth);
      text_drawText(renderer, const_cast<char *>("S to save a file"), 2, 0, 48,
                    visual_whiteText, 0, fontTileCountW);
      text_drawText(renderer, const_cast<char *>("Return to load a file"), 2, 0,
                    64, visual_whiteText, 0, fontTileCountW);
      text_drawText(renderer, const_cast<char *>("ESC to go to parent dir."), 2,
                    0, 80, visual_whiteText, 0, fontTileCountW);
      text_drawText(renderer,
                    const_cast<char *>(
                        (fileMenu_directoryPath + PATH_SEPERATOR_S).c_str()),
                    2, 0, 96, visual_whiteText, 0, INT_MAX);
      unsigned short y = 112;
      int i = 0;
      int initialEntry = std::max(0, static_cast<int>(cursorPosition.y) -
                                         static_cast<int>(fontTileCountH / 2));
      try {
        if (std::filesystem::exists(fileMenu_directoryPath) &&
            std::filesystem::is_directory(fileMenu_directoryPath)) {
          // Iterate through each file in the directory
          for (const auto &entry :
               std::filesystem::directory_iterator(fileMenu_directoryPath)) {
            if (entry.path().filename().string()[0] == '.')
              continue;
            if (i >= initialEntry) {
              text_drawText(
                  renderer,
                  const_cast<char *>(entry.path().filename().string().c_str()),
                  2, 0, y,
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
          fileMenu_errorText =
              const_cast<char *>("Directory not found or invalid.");
        }
      } catch (std::filesystem::filesystem_error &error) {
        fileMenu_errorText =
            const_cast<char *>("Filesystem error reading directory");
      }
      if (i >= initialEntry && y < windowHeight)
        text_drawText(renderer, const_cast<char *>(".."), 2, 0, y,
                      visual_yellowText,
                      i == static_cast<int>(cursorPosition.y), fontTileCountW);
      break;
    }
    case save_file_menu: {
      text_drawText(renderer, const_cast<char *>("Saving to"), 2, 0, 16,
                    visual_whiteText, 0, fontTileCountW);
      text_drawText(renderer,
                    const_cast<char *>(
                        (fileMenu_directoryPath + PATH_SEPERATOR_S).c_str()),
                    2, 160, 16, visual_whiteText, 0, fontTileCountW - 10);
      if (cursorPosition.subMenu == 1)
        text_drawText(renderer,
                      const_cast<char *>("That file exists, are you sure?"), 2,
                      0, 32, visual_redText, 0, fontTileCountW);
      text_drawText(renderer, const_cast<char *>("Type a filename:"), 2, 0, 128,
                    visual_whiteText, 0, fontTileCountW - 10);
      text_drawText(renderer, const_cast<char *>(saveFileMenu_fileName.c_str()),
                    2, 0, 144, visual_whiteText, 0, fontTileCountW - 10);
      break;
    }
    }
  }
}

void sdlLoop(SDL_Renderer *renderer, SDL_Window *window) {
  SDL_Event event;
  int quit = 0;
  while (!quit) {
    while (SDL_PollEvent(&event)) {
      sdlEventHandler(&event, quit);
    }
    screenUpdate(renderer, window);
    SDL_RenderPresent(renderer);
    video_frame++;
  }
}

int main(int argc, char *argv[]) {
  (void)(argc);
  (void)(argv); // These are only here to avoid "undefined reference to
                // SDL_main" errors
#if defined(_POSIX)
  if (std::strcmp(std::getenv("USER"), "root") == 0) {
    std::cerr << "Please don't open this program as root" << std::endl;
    quit(1);
  }
#endif
  if (SDL_InitSubSystem(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS) <
      0) {
    std::cerr << "Failed to start SDL! " << SDL_GetError() << std::endl;
    quit(1);
  }
  int windowWidth = lastWindowWidth = 1024,
      windowHeight = lastWindowHeight = 512;
  SDL_Window *window = SDL_CreateWindow(
      "ChTracker", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth,
      windowHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (window == NULL) {
    std::cerr << "Failed to create a window! " << SDL_GetError() << std::endl;
    quit(1);
  }
  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (renderer == NULL) {
    std::cerr << "Failed to create a renderer! " << SDL_GetError() << std::endl;
    quit(1);
  }
  init(/*renderer, */ window);
  sdlLoop(renderer, window);
  quit(0);
  return 0;
}
