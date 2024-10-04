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

#include <SDL2/SDL_audio.h>
#include <exception>
#define IN_CHTRACKER_CONTEXT

/***************************************
 *                                     *
 *           INCLUDE SECTION           *
 *   ALL #include directives go here   *
 *                                     *
 ***************************************/

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
#include <array>
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
#include "log.hxx"
#include "main.h"
#include "order.hxx"
#include "timer.hxx"

/**************************************
 *                                    *
 *            MACRO SECTION           *
 *   ALL #define DIRECTIVES GO HERE   *
 *                                    *
 **************************************/

#define TILE_SIZE 96
#define TILE_SIZE_F 96.0

/**********************************
 *                                *
 *          USING SECTION         *
 *   ALL using KEYWORDS GO HERE   *
 *                                *
 **********************************/

using std::string;
using std::filesystem::path;

/************************************
 *                                  *
 *     GLOBAL VARIABLES SECTION     *
 *   ALL GLOBAL VARIABLES GO HERE   *
 *                                  *
 ************************************/

/*********
 * Audio *
 *********/

namespace audio {
unsigned long /******/ time = 0;
unsigned char /******/ row = -1;
unsigned short /*****/ pattern = 0;
bool /***************/ isPlaying = false;
bool /***************/ freeze = false;
bool /***************/ isFrozen = true;
unsigned short /*****/ tempo = 960;
SDL_AudioDeviceID /**/ deviceID = 0;
SDL_AudioSpec /******/ spec;
bool /***************/ errorIsPresent = false;
} // namespace audio

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

namespace gui {
std::array<Sint16, WAVEFORM_SAMPLE_COUNT> waveformDisplay;
int /*************/ waveformIdx;
CursorPos /*******/ cursorPosition;
GlobalMenus /*****/ currentMenu = GlobalMenus::main_menu;
unsigned short /**/ patternMenuOrderIndex = 0;
char /************/ patternMenuViewMode = 3;
int /*************/ lastWindowWidth;
int /*************/ lastWindowHeight;
bool /************/ debugMenuUsage;
} // namespace gui

/****************
 * File-related *
 ****************/

#if defined(_WIN32)
path /**/ fileMenu_directoryPath = "C:\\";
#else
path /****/ fileMenu_directoryPath = "/";
#endif
char * /**/ fileMenu_errorText = const_cast<char *>("");
string /**/ saveFileMenu_fileName = "file.cht";
string /**/ renderMenu_fileName = "render.wav";
path /****/ executableAbsolutePath = "";
path /****/ documentationDirectory = "./doc";
bool /****/ global_unsavedChanges = false;

/*****************************
 *                           *
 *     FUNCTIONS SECTION     *
 *   ALL FUNCTIONS GO HERE   *
 *                           *
 *****************************/

void onOpenMenuMain() {
  gui::cursorPosition = {
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
    if (audio::row >= orders.at(0)->at(0)->rowCount() - 1) {
      audio::row = 0;
      if (audio::pattern >= indexes.rowCount() - 1) audio::pattern = 0;
      else audio::pattern++;
    } else audio::row++;
    timerSystem.resetTimer("row", 48000 * 60 / audio::tempo);
    for (unsigned char i = 0; i < instrumentSystem.inst_count(); i++) {
      unsigned char patternIndex = indexes.at(audio::pattern)->at(i);
      row *currentRow = orders.at(i)->at(patternIndex)->at(audio::row);
      instrumentSystem.at(i)->set_row(*currentRow);
    }
  }
  if (timerSystem.isComplete("effect")) {
    for (unsigned char i = 0; i < instrumentSystem.inst_count(); i++) {
      instrumentSystem.at(i)->applyFx();
    }
    timerSystem.resetTimer("effect", 375 * 960 / audio::tempo);
  }
  if (timerSystem.isComplete("arpeggio")) {
    for (unsigned char i = 0; i < instrumentSystem.inst_count(); i++) {
      instrumentSystem.at(i)->applyArpeggio();
    }
    timerSystem.resetTimer("arpeggio", 1500 * 960 / audio::tempo);
  }
}

/*************************
 * Channel name function *
 *************************/

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
  default:
    if (short_name) {
      str = const_cast<char *>("??");
      break;
    };
    str = const_cast<char *>("Unknown");
    break;
  }
  return str;
}

/******************
 * File functions *
 ******************/

int saveFile(path path) {
  cmd::log::notice("Saving to file {}", path.string());
  std::ofstream file(path, std::ios::out | std::ios::binary);
  if (!file || !file.is_open()) {
    cmd::log::error("Couldn't open the file");
    return 1;
  }
  unsigned char *buffer = new unsigned char[256];
  // Version
  for (int i = 0; i < 9; i++) {
    buffer[i] = "CHTRACKER"[i];
  }
  buffer[9] = global_majorVersion;
  buffer[10] = global_minorVersion;
  buffer[11] = global_patchVersion;
  buffer[12] = global_prereleaseVersion;

  buffer[13] = static_cast<unsigned char>(audio::tempo >> 8);
  buffer[14] = static_cast<unsigned char>(audio::tempo & 255);

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
  cmd::log::debug("Wrote header");
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
  cmd::log::debug("Wrote instrument table");
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
            cmd::log::warning("OOR error saving an order index");
            buffer[orderIndex] = 0;
          }
        }
      } catch (std::out_of_range &) {
        cmd::log::warning("OOR error");
        continue;
      }
      try {
        file.write(reinterpret_cast<const char *>(buffer),
                   indexes.at(orderRowIndex)->instCount());
      } catch (std::out_of_range &) {
        cmd::log::debug(

            "OOR error writing data using index-based instrument count");
        file.write(reinterpret_cast<const char *>(buffer), orders.tableCount());
      }
      delete[] buffer;
    }
    cmd::log::debug("Wrote index data");
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
      cmd::log::debug("Wrote order table for instrument {}", tableIdx);
    }
    cmd::log::debug("Wrote order storage");
  }
  file.close();
  cmd::log::debug("Saved successfully");
  return 0;
}

int loadFile(path filePath) {
  if (timerSystem.hasTimer("row"))
    timerSystem.removeTimer("row");
  if (timerSystem.hasTimer("effect"))
    timerSystem.removeTimer("effect");
  if (timerSystem.hasTimer("arpeggio"))
    timerSystem.removeTimer("arpeggio");
  audio::row = 0;
  audio::pattern = 0;
  audio::time = 0;
  audio::isPlaying = false;
  cmd::log::notice("Loading file {}", filePath.string());
  std::ifstream file(filePath, std::ios::in | std::ios::binary);
  if (!file || !file.is_open()) {
    cmd::log::error("Couldn't open the file");
    return 1;
  }
  unsigned char *buffer = new unsigned char[256];
  file.read(reinterpret_cast<char *>(buffer), 256);
  for (int i = 0; i < 9; i++) {
    if (buffer[i] != "CHTRACKER"[i]) {
      fileMenu_errorText = const_cast<char *>("Not a chTRACKER file");
      cmd::log::error(fileMenu_errorText);
      cmd::log::debug("Mismatch on letter {}", i);
      delete[] buffer;
      file.close();
      return 1;
    }
  }
  if (buffer[12] != 0)
    cmd::log::debug("File version {}.{}.{}.{}", static_cast<int>(buffer[9]),
                    static_cast<int>(buffer[10]), static_cast<int>(buffer[11]),
                    static_cast<char>('A' + buffer[12] - 1));
  else
    cmd::log::debug("File version {}.{}.{}", static_cast<int>(buffer[9]),
                    static_cast<int>(buffer[10]), static_cast<int>(buffer[11]));
  if (buffer[9] != global_majorVersion) {
    fileMenu_errorText = const_cast<char *>("Major version mismatch");
    cmd::log::error("Major verion mismatch, expected {} and got {}",
                    global_majorVersion, buffer[9]);
    delete[] buffer;
    file.close();
    return 1;
  }
  if (buffer[10] > global_minorVersion) {
    fileMenu_errorText = const_cast<char *>("Newer minor version");
    cmd::log::error("Newer minor verion, expected {} and got {}",
                    global_minorVersion, buffer[10]);
    delete[] buffer;
    file.close();
    return 1;
  }
  if (buffer[11] > global_patchVersion && buffer[10] == global_minorVersion) {
    cmd::log::warning("Newer patch version, found {}", buffer[11]);
  }
  if (buffer[12] > 0 && global_prereleaseVersion == 0) {
    cmd::log::warning("Opening prerelease file on non-prerelease version");
  }
  unsigned short local_rowsPerMinute = audio::tempo =
      static_cast<unsigned short>(buffer[13]) << 8 |
      static_cast<unsigned short>(buffer[14]);
  unsigned short local_rowsPerPattern = patternLength =
      static_cast<unsigned short>(buffer[15]) << 8 |
      static_cast<unsigned short>(buffer[16]);
  unsigned short local_orderCount = static_cast<unsigned short>(buffer[17])
                                        << 8 |
                                    static_cast<unsigned short>(buffer[18]);
  unsigned char local_instrumentCount = buffer[19];
  delete[] buffer;
  cmd::log::debug("Tempo {}, {} rows per pattern, {} orders and {} instruments",
                  local_rowsPerMinute, local_rowsPerPattern, local_orderCount,
                  local_instrumentCount);
  std::vector<unsigned char> instrumentOrderCounts(0);

  buffer = new unsigned char[local_instrumentCount * 8];
  file.read(reinterpret_cast<char *>(buffer), local_instrumentCount * 8);
  cmd::log::debug("Creating instruments");
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
  cmd::log::debug("Reading index data");
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
  cmd::log::debug("Reading order data");
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
    cmd::log::debug("Read order table for instrument {}", tableIdx);
  }
  file.close();
  cmd::log::debug("File read successfully");
  return 0;
}

int renderTo(path path) {
  cmd::log::notice("Rendering to file {}", path.string());
  if (orders.tableCount() < 1) {
    cmd::log::notice("Refusing to render without instruments");
    return 1;
  }
  std::ofstream file(path, std::ios::out | std::ios::binary);
  if (!file || !file.is_open()) {
    cmd::log::error("Couldn't open the file");
    return 1;
  }
  // Estimate the song length
  size_t songLength = 48000 * 120;
  songLength *= patternLength;
  songLength *= indexes.rowCount();
  songLength /= audio::tempo;
  // Make a buffer whose length is the file size
  size_t fileSize = (songLength * 2) + 44;
  auto buffer = std::make_unique<unsigned char[]>(fileSize);
  size_t headerIdx = 0;
  // Write a WAV header
  {
    // RIFF
    strcpy(reinterpret_cast<char *>(&buffer[0]),
           "RIFF"); // up to 3 (null at 4 must be overwritten)
    headerIdx += 4;
    // File size - 8 (song size + 36)
    write32LE(buffer.get(), fileSize - 8, headerIdx);
    // WAVEfmt\0
    strcpy(reinterpret_cast<char *>(&buffer[headerIdx]),
           "WAVEfmt "); // up to 15 (null at 16 must be overwritten)
    headerIdx += 8;
    // the length of everything before this
    write32LE(buffer.get(), headerIdx, headerIdx);
    // PCM
    write16LE(buffer.get(), 1, headerIdx);
    // Mono
    write16LE(buffer.get(), 1, headerIdx);
    // 48kHz
    {
      constexpr unsigned int sampleRate = 48000;
      write32LE(buffer.get(), sampleRate, headerIdx);
      // Again (sr*16*1)/8 = sr * 2
      // (samplerate * bit depth * channel count) / 8
      write32LE(buffer.get(), sampleRate * 2, headerIdx);
    }
    // 16 bit mono
    write16LE(buffer.get(), 2, headerIdx);
    // 16 bits
    write16LE(buffer.get(), 16, headerIdx);
    // data
    strcpy(reinterpret_cast<char *>(&buffer[36]),
           "data"); // up to 39 (null at 40 must be overwritten)
    headerIdx += 4;
    // Data chunk length
    write32LE(buffer.get(), fileSize - 44, headerIdx);
  }
  cmd::log::debug("WAV header written");
  audio::pattern = 0;
  audio::row = 0;
  audio::freeze = true;
  while (!audio::isFrozen) {
  };

  if (timerSystem.hasTimer("row"))
    timerSystem.removeTimer("row");
  if (timerSystem.hasTimer("effect"))
    timerSystem.removeTimer("effect");
  if (timerSystem.hasTimer("arpeggio"))
    timerSystem.removeTimer("arpeggio");
  timerSystem.addTimer("row", 48000 * 60 / audio::tempo);
  timerSystem.addTimer("effect", 375 * 960 / audio::tempo);
  timerSystem.addTimer("arpeggio", 1500 * 960 / audio::tempo);
  row dummyRow = {rowFeature::note_cut, 'A', 4, 0, std::vector<effect>(0)};
  for (char i = 0; i < 4; i++)
    dummyRow.effects.push_back(effect{effectTypes::arpeggio, 0});
  for (unsigned char i = 0; i < instrumentSystem.inst_count(); i++) {
    unsigned char patternIndex = indexes.at(audio::pattern)->at(i);
    row *currentRow = orders.at(i)->at(patternIndex)->at(audio::row);
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
  audio::freeze = false;
  file.write(reinterpret_cast<char *>(buffer.get()), fileSize);
  file.close();
  cmd::log::debug("Rendered successfully");
  return 0;
}

/*******************
 * Audio callbacks *
 *******************/

void audioCallback(void *userdata, Uint8 *stream, int len) {
  (void)userdata;
  if (audio::errorIsPresent)
    return;
  int samples = len / 2;
  Sint16 *data = reinterpret_cast<Sint16 *>(stream);

  try {
    if (audio::freeze) {
      for (int i = 0; i < samples; i++) {
        data[i] /= 2;
      }
      audio::isFrozen = true;
      return;
    }
    audio::isFrozen = false;

    if (gui::currentMenu == GlobalMenus::main_menu) {
      for (unsigned int i = 0; i < static_cast<unsigned int>(samples); i++) {
        unsigned long t = audio::time + i;
        data[i] = ((((37649&1<<(t>>13&15)?
            (int)((
                (t+314)*128/((t>>8&31)+1)&255
            )/85)*85
            :0)
            |
            ((t*(t>>18&1?t>>17&1?36:38:t>>17&1?32:27)/
                ((2-(2523490710>>(t>>13&31)&1))
                    *(t>>20&1?24:36)
                )&192)))&255)-127)*64;
      }
      audio::time += samples;
      return;
    }

    if (!audio::freeze && audio::isPlaying && orders.tableCount() > 0) {
      if (!timerSystem.hasTimer("row"))
        timerSystem.addTimer("row", audio::spec.freq * 60 / audio::tempo);
      if (!timerSystem.hasTimer("effect"))
        timerSystem.addTimer("effect",
                             audio::spec.freq / 128 * 960 / audio::tempo);
      if (!timerSystem.hasTimer("arpeggio"))
        timerSystem.addTimer("arpeggio",
                             audio::spec.freq / 32 * 960 / audio::tempo);

      for (unsigned int audioIdx = 0;
           audioIdx < static_cast<unsigned int>(samples); audioIdx++) {
        Sint16 sample = 0;
        for (unsigned char instrumentIdx = 0;
             instrumentIdx < instrumentSystem.inst_count(); instrumentIdx++) {
          sample = std::min(
              32767,
              std::max(-32767,
                       static_cast<Sint32>(sample) +
                           static_cast<Sint32>(
                               instrumentSystem.at(instrumentIdx)->gen()) /
                               4));
        }
        data[audioIdx] = gui::waveformDisplay.at(gui::waveformIdx++) = sample;
        if(gui::waveformIdx >= WAVEFORM_SAMPLE_COUNT) gui::waveformIdx = 0;
        audioTickTimers();
        audio::time++;
      }
    } else {
      if (!audio::freeze) {
        if (timerSystem.hasTimer("row"))
          timerSystem.removeTimer("row");
        if (timerSystem.hasTimer("effect"))
          timerSystem.removeTimer("effect");
        if (timerSystem.hasTimer("arpeggio"))
          timerSystem.removeTimer("arpeggio");
        audio::row = 0;
        audio::pattern = gui::patternMenuOrderIndex;
        row dummyRow = {rowFeature::note_cut, 'A', 4, 0,
                        std::vector<effect>(0)};
        for (char i = 0; i < 4; i++)
          dummyRow.effects.push_back(effect{effectTypes::arpeggio, 0});
        for (unsigned char i = 0; i < instrumentSystem.inst_count(); i++) {
          instrumentSystem.at(i)->set_row(dummyRow);
        }
      }
      for (int i = 0; i < samples; i++) {
        data[i] /= 2;
      }
      for (int i = 0; i < WAVEFORM_SAMPLE_COUNT; i++) {
        gui::waveformDisplay.at(i) = 0;
      }
    }
  } catch (std::exception &e) {
    cmd::log::critical("Audio thread: {} {}", typeid(e).name(), e.what());
    audio::errorIsPresent = true;
  }
}

void whoops(void *userdata, Uint8 *stream, int len) {
  (void)userdata;
  for (int i = 0; i < len; i++) {
    unsigned long t = audio::time + i;
    stream[i] =
        37649 & 1 << (t >> 10 & 15)
            ? (int)(((t + 314) * 128 / ((t >> 5 & 31) + 1) & 255) / 85) * 127
            : (t * (t >> 17 & 1 ? 12 : 16) / (4 + (t >> 15 & 3)) *
               (1 + (t >> 13 & 3)));
  }
  audio::time += len;
}

/******************
 * Initialization *
 ******************/

void init() {
#ifdef _POSIX
  char *str = std::getenv("HOME");
  if (str == nullptr) {
    str = std::getenv("USER");
    if (str == nullptr) {
      fileMenu_directoryPath = "/";
    } else {
      fileMenu_directoryPath = string("/home/") + str + "/";
    }
  } else {
    fileMenu_directoryPath = str;
  }
#elif defined(_WIN32)
  fileMenu_directoryPath = executableAbsolutePath.parent_path();
#else
  fileMenu_directoryPath = "/";
#endif
  SDL_AudioSpec preferred;

  preferred.freq = 48000;
  preferred.format = AUDIO_S16;
  preferred.channels = 1;
  preferred.samples = AUDIO_SAMPLE_COUNT;
  preferred.callback = audioCallback;
  audio::deviceID = SDL_OpenAudioDevice(NULL, 0, &preferred, &audio::spec,
                                        SDL_AUDIO_ALLOW_SAMPLES_CHANGE |
                                            SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
  if (audio::deviceID == 0) {
    cmd::log::critical("Couldn't open audio: {}", SDL_GetError());
    quit(1);
  }
  if (audio::spec.samples != preferred.samples)
    cmd::log::notice("Wanted {} sample frames and got {}",
                     AUDIO_SAMPLE_COUNT, audio::spec.samples);
  if (audio::spec.freq != preferred.freq) {
    cmd::log::notice("Wanted {}Hz and got {}Hz, informing audio system",
                     preferred.freq, audio::spec.freq);
    audio::audioChannelFrequency = audio::spec.freq;
  }
  indexes.addRow();
  SDL_PauseAudioDevice(audio::deviceID, 0);
}

/*********************************
 * Cursor and keyboard functions *
 *********************************/

void setLimits(unsigned int &limitX, unsigned int &limitY) {
  switch (gui::currentMenu) {
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
    if (gui::cursorPosition.subMenu == 1 && orders.tableCount() > 0) {
      limitX = orders.tableCount() - 1;
      limitY = 0;
      break;
    }
    limitY = orders.tableCount() - 1;
    if (orders.tableCount() > 0)
      limitX = orders.at(gui::cursorPosition.y)->order_count() - 1;
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
                 gui::patternMenuViewMode)] *
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

#include "../onKeydown.cxx"

void sdlEventHandler(SDL_Event *event, int &quit) {
  unsigned int limitX = INT_MAX;
  unsigned int limitY = INT_MAX;
  setLimits(limitX, limitY);
  if (gui::cursorPosition.x > limitX)
    gui::cursorPosition.x = limitX;
  if (gui::cursorPosition.y > limitY)
    gui::cursorPosition.y = limitY;
  const Uint8 *currentKeyStates = SDL_GetKeyboardState(NULL);
  switch (event->type) {
  case SDL_QUIT:
    quit = 1;
    break;
  case SDL_TEXTINPUT: {
    if (gui::currentMenu == GlobalMenus::save_file_menu)
      saveFileMenu_fileName += event->text.text;
    if (gui::currentMenu == GlobalMenus::render_menu)
      renderMenu_fileName += event->text.text;
    break;
  }
  case SDL_KEYDOWN: {
    onSDLKeyDown(event, quit, gui::currentMenu, gui::cursorPosition,
                 saveFileMenu_fileName, renderMenu_fileName,
                 fileMenu_directoryPath, fileMenu_errorText,
                 global_unsavedChanges, limitX, limitY, audio::freeze,
                 audio::isFrozen, gui::patternMenuOrderIndex,
                 gui::patternMenuViewMode, audio::isPlaying, instrumentSystem,
                 indexes, orders, audio::pattern, patternLength,
                 currentKeyStates, audio::tempo, gui::debugMenuUsage);
    break;
  }
  case SDL_KEYUP: {
    SDL_Keysym ks = event->key.keysym;
    SDL_Keycode code = ks.sym;
    if (gui::currentMenu == GlobalMenus::file_menu) {
      if (code == 's')
        gui::currentMenu = GlobalMenus::save_file_menu;
      else if (code == 'r') {
        if (orders.tableCount() < 1)
          fileMenu_errorText =
              const_cast<char *>("Refusing to render without instruments");
        else
          gui::currentMenu = GlobalMenus::render_menu;
      }
    }
  }
  }
  setLimits(limitX, limitY);
  if (gui::cursorPosition.x > limitX)
    gui::cursorPosition.x = limitX;
  if (gui::cursorPosition.y > limitY)
    gui::cursorPosition.y = limitY;
}

/*************************************************************
 * The giant function that does all of the heavy GUI lifting *
 *              (Which is now in another file)               *
 *************************************************************/

#include "../screenUpdate.cxx"

/**************************
 * Main wrapper functions *
 **************************/

void sdlLoop(SDL_Renderer *renderer, SDL_Window *window) {
  SDL_Event event;
  int quit = 0;
  while (true) {
    if (audio::errorIsPresent)
      throw std::runtime_error("Audio error; check the log (shown below) for details.");
    while (SDL_PollEvent(&event)) {
      sdlEventHandler(&event, quit);
    }
    screenUpdate(renderer, window, gui::lastWindowWidth, gui::lastWindowHeight,
                 gui::currentMenu, audio::isPlaying, gui::waveformDisplay,
                 global_unsavedChanges, gui::cursorPosition, indexes,
                 audio::pattern, audio::row, orders, gui::patternMenuOrderIndex,
                 gui::patternMenuViewMode, instrumentSystem, audio::tempo,
                 patternLength, fileMenu_errorText, fileMenu_directoryPath,
                 saveFileMenu_fileName, renderMenu_fileName,
                 documentationDirectory);
    SDL_RenderPresent(renderer);
    if (quit) {
      if (global_unsavedChanges &&
          gui::currentMenu != GlobalMenus::quit_confirmation_menu) {
        quit = 0;
        gui::currentMenu = GlobalMenus::quit_confirmation_menu;
      } else
        break;
    }
  }
}

void errorScreen(SDL_Renderer *r, SDL_Window *w, const std::exception &e) {
  cmd::log::critical("{} {} Starting the error screen NOW!", typeid(e).name(), e.what());
  cmd::log::critical("Please create an issue at "
                     "https://github.com/Chasyxx/chtracker with "
                     "details of the error and what you were doing when "
                     "the error occurred.");
  try {
    SDL_SetWindowSize(w, 768, 512);
    SDL_PauseAudioDevice(audio::deviceID, 1);
    SDL_CloseAudioDevice(audio::deviceID);
    audio::time = 0;
    SDL_Event event;
    int keys = 0;
    SDL_AudioSpec audioSpec;
    audioSpec.freq = 8000;
    audioSpec.format = AUDIO_U8;
    audioSpec.channels = 1;
    audioSpec.samples = AUDIO_SAMPLE_COUNT;
    audioSpec.callback = whoops;
    audio::deviceID = SDL_OpenAudioDevice(NULL, 0, &audioSpec, NULL,
                                          SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if (audio::deviceID == 0) {
      cmd::log::error("Couldn't open audio during error, leave it! {}",
                      SDL_GetError());
    } else
      SDL_PauseAudioDevice(audio::deviceID, 0);
    while (true) {
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_KEYUP) {
          keys++;
        } else if (event.type == SDL_QUIT)
          keys = 8;
      }
      if (keys > 7)
        break;
      SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
      SDL_RenderClear(r);
      text_drawText(r, "Whoops: chTRACKER has crashed!", 2, 16, 16,
                    visual_redText, 1, 46);
      text_drawText(r, typeid(e).name(), 2, 16, 32, visual_redText, 0, 38);
      text_drawText(r, e.what(), 2, 16, 64, visual_redText, 0, 38);
      text_drawText(r,
                    //"----------==========[[[[[[[[[[]]]]]]]]]]123456"
                    gui::debugMenuUsage
                        ? "Debug menu used; do not report.\n\n"
                          "Press a key up to 8 times to abort."
                        : "Please create an issue at\n"
                          "https://github.com/Chasyxx/chtracker\n"
                          "with details of the error and what you were\n"
                          "doing when the error occurred.\n\n"
                          "Press a key up to 8 times to abort.",
                    2, 16, 128, visual_whiteText, 0, 46);
      for (int i = 0; i < 34; i++) {
        if (static_cast<ssize_t>(cmd::log::logs.size()) - 1 - i < 0)
          break;
        struct log l = cmd::log::logs.at(cmd::log::logs.size() - 1 - i);
        text_drawBigChar(r, indexes_charToIdx(hex(l.severity)), 1, 0,
                         240 + i * 8, visual_whiteText, 1);
        text_drawText(r, l.msg.c_str(), 1, 16, 240 + i * 8, visual_whiteText, 0,
                      256);
      }
      SDL_RenderPresent(r);
    }
  } catch (std::exception &e2) {
    cmd::log::error("{} {} in SDL error display routine!", typeid(e2).name(),
                    e2.what());
  }
  cmd::log::error("Aborting!");
  abort();
}

/**************************
 * Command-line functions *
 **************************/

void printHelp() {
#define pH(x) std::cout << x
  pH("Usage: " << executableAbsolutePath << " [-l 0-4] [-v] [FILE]"
               << "\n\n");
  pH("-l --loglevel: Change loglevel. Lower is more verbose"
     << "\n");
  pH("-v --verbose : increase verbosity"
     << "\n\n");
  pH("if FILE is included, load it automatically." << std::endl);
#undef pH
}

void processCommandLineArgumentBeforeInit(const char **argv,
                                          std::string &argument, int &p) {
  if (argument.at(0) == '-') {
    if (argument == "--loglevel" || argument == "-l") {
      cmd::log::level = atoi(argv[++p]);
    } else if (argument == "--verbose" || argument == "-v") {
      if (cmd::log::level > 0)
        cmd::log::level--;
    } else if (argument == "--help" || argument == "-?" || argument == "-h") {
      printHelp();
      exit(0);
    }
  }
}

void processCommandLineArgumentAfterInit(const char **argv,
                                         std::string &argument, int &p) {
  if (argument.at(0) == '-') {
    if (argument == "--loglevel" || argument == "-l") {
      p++;
    } else if (argument == "--verbose" || argument == "-v") {
    } else {
      printHelp();
      cmd::log::critical("Unknown option " + argument);
      quit(1);
    }
  } else {
    path loadFilePath = argv[p];
    if (loadFile(loadFilePath)) {
      gui::currentMenu = GlobalMenus::file_menu;
      onOpenMenuMain();
      if (fileMenu_errorText[0] == 0)
        fileMenu_errorText = const_cast<char *>("Couldn't auto-load file");
    } else {
      gui::currentMenu = GlobalMenus::pattern_menu;
      onOpenMenuMain();
    }
  }
}

/*****************
 * Main function *
 *****************/

int main(int argc, char *argv[]) {
#if defined(__linux__)
  {
    int protector = 0;
    executableAbsolutePath = std::filesystem::read_symlink("/proc/self/exe");
    while (std::filesystem::is_symlink(executableAbsolutePath)) {
      executableAbsolutePath =
          std::filesystem::read_symlink(executableAbsolutePath);
      protector++;
      if (protector >= 64) {
        cmd::log::warning("Too many symlinks to process executable location");
        break;
      }
    }
  }
  documentationDirectory = executableAbsolutePath.parent_path() / "doc";
#elif defined(_WIN32)
  { executableAbsolutePath = _pgmptr; }
  documentationDirectory = executableAbsolutePath.parent_path() / "doc";
#endif
  if (argc > 1) {
    for (int p = 1; p < argc; p++) {
      string arg(argv[p]);
      processCommandLineArgumentBeforeInit(const_cast<const char **>(argv), arg,
                                           p);
    }
  }
#if defined(_POSIX)
  if (std::strcmp(std::getenv("USER"), "root") == 0) {
    cmd::log::warning("Please don't run this program as root");
    quit(1);
  }
#endif
  cmd::log::notice("Welcome to chTRACKER!");
  cmd::log::debug("The program path is {}", executableAbsolutePath.string());
  cmd::log::debug("The dynamic docdir is {}", documentationDirectory.string());
  cmd::log::debug("Starting SDL");
  if (SDL_InitSubSystem(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS) <
      0) {
    cmd::log::critical("Failed to start SDL: {}", SDL_GetError());
    quit(1);
  }
  int windowWidth = gui::lastWindowWidth = 1024,
      windowHeight = gui::lastWindowHeight = 512;
  cmd::log::debug("Creating window");
  SDL_Window *window = SDL_CreateWindow(
      "ChTracker", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth,
      windowHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (window == NULL) {
    cmd::log::critical("Failed to create a window: ", SDL_GetError());
    quit(1);
  }
  cmd::log::debug("Creating renderer");
  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (renderer == NULL) {
    cmd::log::critical("Failed to create a renderer: {}", SDL_GetError());
    quit(1);
  }
  init();
  if (argc > 1) {
    for (int p = 1; p < argc; p++) {
      string arg(argv[p]);
      processCommandLineArgumentAfterInit(const_cast<const char **>(argv), arg,
                                          p);
    }
  }
#ifdef DEBUG
  sdlLoop(renderer, window);
#else
  try {
    sdlLoop(renderer, window);
  } catch (std::exception &error) {
    errorScreen(renderer, window, error);
  }
#endif
  quit(0);
  return 0;
}

#undef IN_CHTRACKER_CONTEXT
