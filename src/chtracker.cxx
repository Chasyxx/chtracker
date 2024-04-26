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

/***********************************
 *         INCLUDE SECTION         *
 * All #include directives go here *
 ***********************************/

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

#include "channel.hxx"
#include "main.h"
#include "order.hxx"
#include "timer.hxx"

/**************************************
 *                                    *
 *            MACRO SECTION           *
 *   All #define DIRECTIVES GO HERE   *
 *                                    *
 **************************************/

#define TILE_SIZE 96
#define TILE_SIZE_F 96.0

#if defined(_WIN32)
#define PATH_SEPERATOR_S "\\"
#define PATH_SEPERATOR '\\'
#elif defined(__linux__) || defined(__APPLE__)
#define PATH_SEPERATOR_S "/"
#define PATH_SEPERATOR '/'
#define _POSIX
#else
#define PATH_SEPERATOR_S "/"
#define PATH_SEPERATOR '/'
#define HOPEFULLY_POSIX
#endif

/************************************
 *                                  *
 *     GLOBAL VARIABLES SECTION     *
 *   All GLOBAL VARIABLES GO HERE   *
 *                                  *
 ************************************/

/*********
 * Audio *
 *********/

unsigned long /***/ audio_time = 0;
unsigned char /***/ audio_row = -1;
unsigned short /**/ audio_pattern = 0;
bool /************/ audio_isPlaying = false;
bool /************/ audio_freeze = false;
bool /************/ audio_isFrozen = true;
unsigned short /**/ audio_tenpo = 960;

/**************
 * Music data *
 **************/

orderStorage /*******/ orders(32);
orderIndexStorage /**/ indexes;
unsigned short /*****/ patternLength = 32;

/***********
 * Systems *
 ***********/

timerHandler /**/ timerSystem;
instrumentStorage instrumentSystem;

/*********
 * (G)UI *
 *********/

CursorPos /*******/ cursorPosition;
GlobalMenus /*****/ global_currentMenu = GlobalMenus::main_menu;
unsigned short /**/ patternMenu_orderIndex = 0;
char /************/ patternMenu_viewMode = 3;
int /*************/ lastWindowWidth;
int /*************/ lastWindowHeight;
Sint16 /**********/ waveformDisplay[AUDIO_SAMPLE_COUNT];

/****************
 * File-related *
 ****************/

#if defined(_WIN32)
std::string /**/ fileMenu_directoryPath = "C:\\";
#else
std::string /**/ fileMenu_directoryPath = "/";
#endif
char * /*******/ fileMenu_errorText = const_cast<char *>("");
std::string /**/ saveFileMenu_fileName = "file.cht";
std::string /**/ renderMenu_fileName = "render.wav";
bool /*********/ global_unsavedChanges = false;

/*****************************
 *                           *
 *     FUNCTIONS SECTION     *
 *   ALL FUNCTIONS GO HERE   *
 *                           *
 *****************************/

void onOpenMenu() {
  cursorPosition = {
      .x = 0, .y = 0, .subMenu = 0, .selection = {.x = 0, .y = 0}};
}
// Quit SDL and terminate with code.
void quit(int code = 0) {
  SDL_Quit();
  exit(code);
}

/***********************************
 * Data functions (for wav header) *
 ***********************************/

void write32LE(unsigned char *buffer, unsigned int a, size_t &bufferIdx) {
  buffer[bufferIdx++] = (static_cast<unsigned char>(a & 255));
  buffer[bufferIdx++] = (static_cast<unsigned char>(a >> 8 & 255));
  buffer[bufferIdx++] = (static_cast<unsigned char>(a >> 16 & 255));
  buffer[bufferIdx++] = (static_cast<unsigned char>(a >> 24 & 255));
}

void write16LE(unsigned char *buffer, unsigned short a, size_t &bufferIdx) {
  buffer[bufferIdx++] = (static_cast<unsigned char>(a & 255));
  buffer[bufferIdx++] = (static_cast<unsigned char>(a >> 8 & 255));
}

/***********************
 * Audio timer handler *
 ***********************/

void audioTickTimers() {
  timerSystem.tick();
  if (timerSystem.isComplete("row")) {
    audio_row++;
    if (audio_row >= orders.at(0)->at(0)->rowCount()) {
      audio_row = 0;
      audio_pattern++;
      if (audio_pattern >= indexes.rowCount()) {
        audio_pattern = 0;
      }
    }
    timerSystem.resetTimer("row", 48000 * 60 / audio_tenpo);
    for (unsigned char i = 0; i < instrumentSystem.inst_count(); i++) {
      unsigned char patternIndex = indexes.at(audio_pattern)->at(i);
      row *currentRow = orders.at(i)->at(patternIndex)->at(audio_row);
      instrumentSystem.at(i)->set_row(*currentRow);
    }
  }
  if (timerSystem.isComplete("effect")) {
    for (unsigned char i = 0; i < instrumentSystem.inst_count(); i++) {
      instrumentSystem.at(i)->applyFx();
    }
    timerSystem.resetTimer("effect", 375 * 960 / audio_tenpo);
  }
  if (timerSystem.isComplete("arpeggio")) {
    for (unsigned char i = 0; i < instrumentSystem.inst_count(); i++) {
      instrumentSystem.at(i)->applyArpeggio();
    }
    timerSystem.resetTimer("arpeggio", 1500 * 960 / audio_tenpo);
  }
}

/******************
 * File functions *
 ******************/

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

  buffer[13] = static_cast<unsigned char>(audio_tenpo >> 8);
  buffer[14] = static_cast<unsigned char>(audio_tenpo & 255);

  buffer[15] = static_cast<unsigned char>(patternLength >> 8);
  buffer[16] = static_cast<unsigned char>(patternLength & 255);

  {
    unsigned short orderCount = indexes.rowCount();
    buffer[17] = static_cast<unsigned char>(orderCount >> 8);
    buffer[18] = static_cast<unsigned char>(orderCount & 255);
  }
  buffer[19] = instrumentSystem.inst_count();

  // Fill unused space with FF
  for (int i = 20; i < 256; i++) {
    buffer[i] = 0xff;
  }
  file.write(reinterpret_cast<const char *>(buffer), 256);
  delete[] buffer;
  for (int i = 0; i < instrumentSystem.inst_count(); i++) {
    buffer = new unsigned char[8];
    buffer[0] = static_cast<unsigned char>(instrumentSystem.at(i)->get_type());
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
  if (timerSystem.hasTimer("row"))
    timerSystem.removeTimer("row");
  if (timerSystem.hasTimer("effect"))
    timerSystem.removeTimer("effect");
  if (timerSystem.hasTimer("arpeggio"))
    timerSystem.removeTimer("arpeggio");
  audio_row = 0;
  audio_pattern = 0;
  audio_time = 0;
  audio_isPlaying = false;
  std::ifstream file(filePath, std::ios::in | std::ios::binary);
  if (!file || !file.is_open())
    return 1;
  unsigned char *buffer = new unsigned char[256];
  file.read(reinterpret_cast<char *>(buffer), 256);
  for (int i = 0; i < 9; i++) {
    if (buffer[i] != "CHTRACKER"[i]) {
      fileMenu_errorText = const_cast<char *>("Not a chTRACKER file");
      delete[] buffer;
      file.close();
      return 1;
    }
  }
  if (buffer[9] != global_majorVersion) {
    fileMenu_errorText = const_cast<char *>("Major version mismatch");
    delete[] buffer;
    file.close();
    return 1;
  }
  if (buffer[10] > global_minorVersion) {
    fileMenu_errorText = const_cast<char *>("Newer minor version");
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
  while (instrumentSystem.inst_count() > 0)
    instrumentSystem.remove_inst(instrumentSystem.inst_count() - 1);
  for (unsigned char instrumentIdx = 0; instrumentIdx < local_instrumentCount;
       instrumentIdx++) {
    unsigned char *i = buffer + (instrumentIdx * 8);
    instrumentSystem.add_inst(static_cast<audioChannelType>(i[0]));
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

  audio_tenpo = local_rowsPerMinute;
  patternLength = local_rowsPerPattern;
  file.close();
  return 0;
}

int renderTo(std::filesystem::path path) {
  if (orders.tableCount() < 1)
    return 1;
  std::ofstream file(path, std::ios::out | std::ios::binary);
  if (!file || !file.is_open())
    return 1;
  // Estimate the song length
  size_t songLength = 48000 * 120;
  songLength *= patternLength;
  songLength *= indexes.rowCount();
  songLength /= audio_tenpo;
  // Make a buffer whose length is the file size
  size_t fileSize = (songLength * 2) + 44;
  unsigned char *buffer = new unsigned char[fileSize];
  size_t headerIdx = 0;
  // Write a WAV header
  {
    // RIFF
    strcpy(reinterpret_cast<char *>(&buffer[0]),
           "RIFF"); // up to 3 (null at 4 must be overwritten)
    headerIdx += 4;
    // File size - 8 (song size + 36)
    write32LE(buffer, fileSize - 8, headerIdx);
    // WAVEfmt\0
    strcpy(reinterpret_cast<char *>(&buffer[headerIdx]),
           "WAVEfmt "); // up to 15 (null at 16 must be overwritten)
    headerIdx += 8;
    // the length of everything before this
    write32LE(buffer, headerIdx, headerIdx);
    // PCM
    write16LE(buffer, 1, headerIdx);
    // Mono
    write16LE(buffer, 1, headerIdx);
    // 48kHz
    {
      constexpr unsigned int sampleRate = 48000;
      write32LE(buffer, sampleRate, headerIdx);
      // Again (sr*16*1)/8 = sr * 2
      // (samplerate * bit depth * channel count) / 8
      write32LE(buffer, sampleRate * 2, headerIdx);
    }
    // 16 bit mono
    write16LE(buffer, 2, headerIdx);
    // 16 bits
    write16LE(buffer, 16, headerIdx);
    // data
    strcpy(reinterpret_cast<char *>(&buffer[36]),
           "data"); // up to 39 (null at 40 must be overwritten)
    headerIdx += 4;
    // Data chunk length
    write32LE(buffer, fileSize - 44, headerIdx);
  }
  audio_pattern = 0;
  audio_row = 0;
  audio_freeze = true;
  while (!audio_isFrozen) {
  };

  if (timerSystem.hasTimer("row"))
    timerSystem.removeTimer("row");
  if (timerSystem.hasTimer("effect"))
    timerSystem.removeTimer("effect");
  if (timerSystem.hasTimer("arpeggio"))
    timerSystem.removeTimer("arpeggio");
  timerSystem.addTimer("row", 48000 * 60 / audio_tenpo);
  timerSystem.addTimer("effect", 375 * 960 / audio_tenpo);
  timerSystem.addTimer("arpeggio", 1500 * 960 / audio_tenpo);
  row dummyRow = {rowFeature::note_cut, 'A', 4, 0, std::vector<effect>(0)};
  for (char i = 0; i < 4; i++)
    dummyRow.effects.push_back(effect{effectTypes::arpeggio, 0});
  for (unsigned char i = 0; i < instrumentSystem.inst_count(); i++) {
    unsigned char patternIndex = indexes.at(audio_pattern)->at(i);
    row *currentRow = orders.at(i)->at(patternIndex)->at(audio_row);
    instrumentSystem.at(i)->set_row(dummyRow);
    instrumentSystem.at(i)->set_row(*currentRow);
  }
  for (unsigned int audioIdx = 0; audioIdx < songLength; audioIdx++) {
    short sample = 0;
    for (unsigned char instrumentIdx = 0;
         instrumentIdx < instrumentSystem.inst_count(); instrumentIdx++) {
      sample = std::min(
          32767,
          std::max(-32767, static_cast<Sint32>(sample) +
                               static_cast<Sint32>(
                                   instrumentSystem.at(instrumentIdx)->gen()) /
                                   4));
    }
    buffer[audioIdx * 2 + 44] = sample << 8;
    buffer[audioIdx * 2 + 45] = sample >> 8;
    audioTickTimers();
  }
  if (timerSystem.hasTimer("row"))
    timerSystem.removeTimer("row");
  if (timerSystem.hasTimer("effect"))
    timerSystem.removeTimer("effect");
  if (timerSystem.hasTimer("arpeggio"))
    timerSystem.removeTimer("arpeggio");
  audio_freeze = false;
  file.write(reinterpret_cast<char *>(buffer), fileSize);
  file.close();
  delete[] buffer;
  return 0;
}

/******************
 * Audio callback *
 ******************/

void audioCallback(void *userdata, Uint8 *stream, int len) {
  (void)userdata;
  int samples = len / 2;
  Sint16 *data = reinterpret_cast<Sint16 *>(stream);

  if (audio_freeze) {
    for (int i = 0; i < samples; i++) {
      data[i] /= 2;
    }
    audio_isFrozen = true;
    return;
  }
  audio_isFrozen = false;

  if (global_currentMenu == GlobalMenus::main_menu) {
    for (unsigned int i = 0; i < static_cast<unsigned int>(samples); i++) {
      unsigned long t = audio_time + i;
      data[i] =
          (-(3 * t >> 5 & t >> 14 & t >> 6) * t << 1) | (t * 5 * 128 & t << 1);
    }
    audio_time += samples;
    return;
  }

  if (!audio_freeze && audio_isPlaying && orders.tableCount() > 0) {
    if (!timerSystem.hasTimer("row"))
      timerSystem.addTimer("row", 48000 * 60 / audio_tenpo);
    if (!timerSystem.hasTimer("effect"))
      timerSystem.addTimer("effect", 375 * 960 / audio_tenpo);
    if (!timerSystem.hasTimer("arpeggio"))
      timerSystem.addTimer("arpeggio", 1500 * 960 / audio_tenpo);

    for (unsigned int audioIdx = 0;
         audioIdx < static_cast<unsigned int>(samples); audioIdx++) {
      Sint16 sample = 0;
      for (unsigned char instrumentIdx = 0;
           instrumentIdx < instrumentSystem.inst_count(); instrumentIdx++) {
        sample = std::min(
            32767, std::max(-32767,
                            static_cast<Sint32>(sample) +
                                static_cast<Sint32>(
                                    instrumentSystem.at(instrumentIdx)->gen()) /
                                    4));
      }
      data[audioIdx] = waveformDisplay[audioIdx] = sample;
      audioTickTimers();
      audio_time++;
    }
  } else {
    if (!audio_freeze) {
      if (timerSystem.hasTimer("row"))
        timerSystem.removeTimer("row");
      if (timerSystem.hasTimer("effect"))
        timerSystem.removeTimer("effect");
      if (timerSystem.hasTimer("arpeggio"))
        timerSystem.removeTimer("arpeggio");
      audio_row = 0;
      audio_pattern = patternMenu_orderIndex;
      row dummyRow = {rowFeature::note_cut, 'A', 4, 0, std::vector<effect>(0)};
      for (char i = 0; i < 4; i++)
        dummyRow.effects.push_back(effect{effectTypes::arpeggio, 0});
      for (unsigned char i = 0; i < instrumentSystem.inst_count(); i++) {
        instrumentSystem.at(i)->set_row(dummyRow);
      }
    }
    for (int i = 0; i < samples; i++) {
      data[i] /= 2;
      waveformDisplay[i] = 0;
    }
  }
}

/******************
 * Initialization *
 ******************/

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
  audioSpec.format = AUDIO_S16;
  audioSpec.channels = 1;
  audioSpec.samples = AUDIO_SAMPLE_COUNT;
  audioSpec.callback = audioCallback;

  if (SDL_OpenAudio(&audioSpec, NULL) < 0) {
    std::cerr << "Couldn't open audio! " << SDL_GetError() << std::endl;
    quit(1);
  }
  indexes.addRow();
  SDL_PauseAudio(0);
}

/*********************************
 * Cursor and keyboard functions *
 *********************************/

void setLimits(unsigned int &limitX, unsigned int &limitY) {
  switch (global_currentMenu) {
  case GlobalMenus::instrument_menu: {
    limitX = 0;
    limitY = instrumentSystem.inst_count() - 1;
    break;
  }
  case GlobalMenus::order_menu: {
    limitX = indexes.instCount(0) - 1;
    limitY = indexes.rowCount() - 1;
    break;
  }
  case GlobalMenus::order_management_menu: {
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
  case GlobalMenus::pattern_menu: {
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
  case GlobalMenus::options_menu: {
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
    if (global_currentMenu == GlobalMenus::save_file_menu)
      saveFileMenu_fileName += event->text.text;
    if (global_currentMenu == GlobalMenus::render_menu)
      renderMenu_fileName += event->text.text;
    break;
  }
  case SDL_KEYDOWN: {
    SDL_Keysym ks = event->key.keysym;
    SDL_Keycode code = ks.sym;
    if (global_currentMenu == GlobalMenus::save_file_menu ||
        global_currentMenu == GlobalMenus::render_menu) {
      if (code == SDLK_ESCAPE) {
        global_currentMenu = GlobalMenus::file_menu;
        onOpenMenu();
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
            break;
          }
          if (global_currentMenu == GlobalMenus::save_file_menu) {
            int exit_code = saveFile(path);
            if (exit_code) {
              fileMenu_errorText =
                  const_cast<char *>("Refused to save the file");
              global_currentMenu = GlobalMenus::file_menu;
              onOpenMenu();
            } else {
              global_currentMenu = GlobalMenus::pattern_menu;
              onOpenMenu();
              global_unsavedChanges = false;
            };
          } else {
            int exit_code = renderTo(path);
            if (exit_code) {
              fileMenu_errorText =
                  const_cast<char *>("Refused to render to the file");
              global_currentMenu = GlobalMenus::file_menu;
              onOpenMenu();
            } else {
              global_currentMenu = GlobalMenus::pattern_menu;
              onOpenMenu();
            };
          }
        } catch (std::filesystem::filesystem_error &) {
          fileMenu_errorText = const_cast<char *>(
              "Filesystem error trying to save/render to the file");
          global_currentMenu = GlobalMenus::file_menu;
          onOpenMenu();
        }
      }
      break;
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
        onOpenMenu();
        break;
      }
      case SDLK_F2: {
        global_currentMenu = GlobalMenus::order_menu;
        onOpenMenu();
        break;
      }
      case SDLK_F3: {
        global_currentMenu = GlobalMenus::pattern_menu;
        onOpenMenu();
        break;
      }
      case SDLK_F4: {
        global_currentMenu = GlobalMenus::instrument_menu;
        onOpenMenu();
        break;
      }
      case SDLK_F5: {
        global_currentMenu = GlobalMenus::order_management_menu;
        onOpenMenu();
        break;
      }
      case SDLK_F6: {
        global_currentMenu = GlobalMenus::options_menu;
        onOpenMenu();
        break;
      }
      case SDLK_F7: {
        global_currentMenu = GlobalMenus::file_menu;
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
                      onOpenMenu();
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
      break;
    }
    break;
  }
  case SDL_KEYUP: {
    SDL_Keysym ks = event->key.keysym;
    SDL_Keycode code = ks.sym;
    if (global_currentMenu == GlobalMenus::file_menu) {
      if (code == 's')
        global_currentMenu = GlobalMenus::save_file_menu;
      else if (code == 'r') {
        if (orders.tableCount() < 1)
          fileMenu_errorText =
              const_cast<char *>("Refusing to render without instruments");
        else
          global_currentMenu = GlobalMenus::render_menu;
      }
    }
  }
  }
  setLimits(limitX, limitY);
  if (cursorPosition.x > limitX)
    cursorPosition.x = limitX;
  if (cursorPosition.y > limitY)
    cursorPosition.y = limitY;
}

/*************************************************************
 * The giant function that does all of the heavy GUI lifting
 * (Which is now in another file) *
 *************************************************************/

#include "../screenUpdate.cxx"

/**************************
 * Main wrapper functions *
 **************************/

void sdlLoop(SDL_Renderer *renderer, SDL_Window *window) {
  SDL_Event event;
  int quit = 0;
  while (true) {
    while (SDL_PollEvent(&event)) {
      sdlEventHandler(&event, quit);
    }
    screenUpdate(renderer, window, lastWindowWidth, lastWindowHeight,
                 global_currentMenu, audio_isPlaying, waveformDisplay,
                 global_unsavedChanges, cursorPosition, indexes, audio_pattern,
                 audio_row, orders, patternMenu_orderIndex,
                 patternMenu_viewMode, instrumentSystem, audio_tenpo,
                 patternLength, fileMenu_errorText, fileMenu_directoryPath,
                 saveFileMenu_fileName, renderMenu_fileName);screenUpdate(renderer, window, lastWindowWidth, lastWindowHeight, 
        global_currentMenu, audio_isPlaying, 
        waveformDisplay, global_unsavedChanges, cursorPosition, 
        indexes, audio_pattern, audio_row, orders, 
        patternMenu_orderIndex, 
        patternMenu_viewMode, instrumentSystem, 
        audio_tenpo, patternLength, fileMenu_errorText, 
        fileMenu_directoryPath, saveFileMenu_fileName, 
        renderMenu_fileName);
    SDL_RenderPresent(renderer);
    if (quit) {
      if (global_unsavedChanges &&
          global_currentMenu != GlobalMenus::quit_connfirmation_menu) {
        quit = 0;
        global_currentMenu = GlobalMenus::quit_connfirmation_menu;
      } else
        break;
    }
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
