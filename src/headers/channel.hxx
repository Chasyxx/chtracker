/*
  Copyright (C) 2024 Chase Taylor
  
  This file is part of chTRACKER at path ./src/headers/channel.hxx
  This is a declaration file; For implementation see path
  ./src/channel/channel.cxx

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

#ifndef LIBRARY_CHANNEL
#define LIBRARY_CHANNEL

#include "order.hxx"
#include <vector>

enum class audioChannelType {
    null,
    lfsr8,
    lfsr14,
    pulse,
    triangle,
    sawtooth
};

struct audioChannelEffectFlags {
    int pitchOffset = 0;
    int volumeOffset = 0;
    unsigned short instrumentVariation;
    char arpeggio[3];
    char arpeggioIndex;
    char arpeggioSubIndex;
    char arpeggioSpeed;
};

class audioChannel {
    private:
        unsigned short noiseLFSR = 1;
        row status;
        audioChannelType channelType = audioChannelType::null;
        struct audioChannelEffectFlags effects;
        float phase = 0;
    public:
    audioChannel(audioChannelType type);
    void cycle_type();
    audioChannelType get_type();
    void set_row(row r);
    void noiseLFSRTick(char width);
    void applyFx();
    void applyArpeggio();
    short gen();
};

class instrumentStorage {
    private:
    std::vector<audioChannel> instruments;
    std::vector<audioChannel> erase_instruments;
    public:
    instrumentStorage();
    unsigned char add_inst(audioChannelType type);
    unsigned char inst_count();
    void remove_inst(unsigned char idx);
    audioChannel* at(unsigned char idx);
};

#endif