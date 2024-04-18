/*
  Copyright (C) 2024 Chase Taylor

  This file is part of chTRACKER at path ./src/channel/channel.cxx

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

#include "order.hxx"
#include "channel.hxx"
#include <climits>
#include <cmath>
#include <stdexcept>
#include <vector>

audioChannel::audioChannel(audioChannelType type): 
    noiseLFSR(1), 
    status({rowFeature::empty, 'A',
        4, 255, std::vector<effect>(4)}),
    channelType(type), effects({ 
        .pitchOffset=0, 
        .volumeOffset=0, 
        .instrumentVariation=USHRT_MAX / 2, 
        .arpeggio = {0,0,0},
        .arpeggioIndex = 0,
        .arpeggioSubIndex = 0,
        .arpeggioSpeed = 0
    }), phase(0) { }

void audioChannel::cycle_type() {
    switch(channelType) {
        case audioChannelType::null: channelType = audioChannelType::lfsr8; break;
        case audioChannelType::lfsr8: channelType = audioChannelType::lfsr14; break;
        case audioChannelType::lfsr14: channelType = audioChannelType::pulse; break;
        case audioChannelType::pulse: channelType = audioChannelType::triangle; break;
        case audioChannelType::triangle: channelType = audioChannelType::sawtooth; break;
        case audioChannelType::sawtooth: channelType = audioChannelType::lfsr8; break;
    }
}

audioChannelType audioChannel::get_type() {
    return channelType;
}

void audioChannel::set_row(row r) {
    if(r.feature != rowFeature::empty) {
        for(unsigned char i = 0; i < 4; i++) {
            effect& e = status.effects.at(i);
            if(e.type == effectTypes::pitchDown 
            || e.type == effectTypes::pitchUp
            || e.type == effectTypes::volumeDown
            || e.type == effectTypes::volumeUp) e.type = effectTypes::null;
        }
        effects.volumeOffset = 0;
        effects.pitchOffset = 0;
        effects.arpeggioIndex = 0;
        effects.arpeggioSubIndex = 0;

        switch(r.feature) {
            case rowFeature::empty: {
                return;
            }
            case rowFeature::note_cut: {
                phase = 0;
                noiseLFSR = 1;
                status.feature = rowFeature::empty;
                status.volume = 0;
                break;
            }
            case rowFeature::note: {
                phase = 0;
                status.feature = rowFeature::note;
                status.note = r.note;
                status.octave = r.octave;
                status.volume = r.volume;
                break;
            }
        }
    }
    for(unsigned char i = 0; i < r.effects.size() && i < 4; i++) {
        effect& rowEffect = r.effects.at(i);
        effect& statusEffect = status.effects.at(i);

        if(rowEffect.type == effectTypes::null) continue;
        if(rowEffect.effect == 0) {
            statusEffect.type = effectTypes::null;
            statusEffect.effect = 0;
            if(rowEffect.type == effectTypes::arpeggio) {
                effects.arpeggio[0] = 
                effects.arpeggio[1] = 
                effects.arpeggio[2] = 0;
            }
            continue;
        }
        statusEffect.type = rowEffect.type;
        statusEffect.effect = rowEffect.effect;
    }
}

void audioChannel::noiseLFSRTick(char width) {
    unsigned char tap = (noiseLFSR&1)^((noiseLFSR>>1)&1);
    noiseLFSR>>=1;
    noiseLFSR|=tap<<width;
    if(noiseLFSR==0) noiseLFSR = 1;
}

void audioChannel::applyFx() {
    switch(channelType) {
        default: break;
        case audioChannelType::pulse: effects.instrumentVariation = USHRT_MAX / 2; break;
        case audioChannelType::triangle: effects.instrumentVariation = 0; break;
        case audioChannelType::sawtooth: effects.instrumentVariation = 0; break;
    }
    for(unsigned char effectIndex = 0; effectIndex<4; effectIndex++) {
        effect& e = status.effects.at(effectIndex);
        switch(e.type) {
            case effectTypes::null: break;
            case effectTypes::arpeggio: {
                effects.arpeggioSpeed = (e.effect&15) + 1;
                effects.arpeggio[0] =(e.effect>>12)&15;
                effects.arpeggio[1] =(e.effect>>8)&15;
                effects.arpeggio[2] =(e.effect>>4)&15;
                break;
            };
            case effectTypes::pitchDown: {
                if(effects.pitchOffset > (INT_MIN / 2)) effects.pitchOffset-=e.effect;
                break;
            }
            case effectTypes::pitchUp: {
                if(effects.pitchOffset < (INT_MAX / 2)) effects.pitchOffset+=e.effect;
                break;
            }
            case effectTypes::volumeDown: {
                if(effects.volumeOffset > (INT_MIN / 2)) effects.volumeOffset-=e.effect;
                break;
            }
            case effectTypes::volumeUp: {
                if(effects.volumeOffset < (INT_MAX / 2)) effects.volumeOffset+=e.effect;
                break;
            }
            case effectTypes::instrumentVariation: {
                effects.instrumentVariation = e.effect;
            }
        }
    }
}

void audioChannel::applyArpeggio() {
    if(++effects.arpeggioSubIndex >= effects.arpeggioSpeed) {
        effects.arpeggioSubIndex=0;
        effects.arpeggioIndex=(effects.arpeggioIndex+1)&3;
    }
}

unsigned char audioChannel::gen() {
    if(status.feature == rowFeature::empty) return 0;
    float Hz = std::pow(2,(status.note-'A'-48+(effects.arpeggioIndex==0?0:effects.arpeggio[effects.arpeggioIndex-1]))/12.0+status.octave)*440;
    Hz += effects.pitchOffset/512.0;
    if(Hz<=0) return 0; // DC or reverse; we don't want reverse!
    unsigned char final_volume = static_cast<unsigned char>(static_cast<unsigned short>( std::min(255.0, std::max(0.0, 255.0+effects.volumeOffset/2048.0)*status.volume/255.0 )));
    float change = 1.0/48000.0*Hz;
    if(channelType == audioChannelType::lfsr8) phase+=change;
    else if(channelType == audioChannelType::lfsr14) phase+=change;
    else phase=static_cast<float>(std::fmod(phase+change,1.0));
    switch(channelType) {
        case audioChannelType::null: return 0;
        case audioChannelType::pulse: return static_cast<unsigned short>(phase*USHRT_MAX)>effects.instrumentVariation?final_volume:0;
        case audioChannelType::lfsr8: {
            while(phase>1) {
                phase-=1;
                noiseLFSRTick(8);
            }
            return (noiseLFSR&1)*final_volume;
        }
        case audioChannelType::lfsr14: {
            while(phase>1) {
                phase-=1;
                noiseLFSRTick(14);
            }
            return (noiseLFSR&1)*final_volume;
        }
        case audioChannelType::triangle: return static_cast<unsigned char>(std::fmod(std::abs(phase*(effects.instrumentVariation/16384.0+2.0)-1.0),1.0)*final_volume);
        case audioChannelType::sawtooth: return static_cast<unsigned char>(
            static_cast<float>(
                (static_cast<unsigned short>(phase*USHRT_MAX)&~effects.instrumentVariation)^
                (static_cast<unsigned short>(std::fmod(phase*1.5,1.0)*USHRT_MAX)&effects.instrumentVariation)
            )/USHRT_MAX*final_volume
        );
    }
    return 0;
}

// instrumentStorage

instrumentStorage::instrumentStorage() {}
unsigned char instrumentStorage::add_inst(audioChannelType type) {
    instruments.push_back(audioChannel(type));
    return instruments.size()-1;
}
unsigned char instrumentStorage::inst_count() {
    return instruments.size();
}
void instrumentStorage::remove_inst(unsigned char idx) {
    if(idx>=inst_count()) throw std::out_of_range("orderStorage::remove_table");
    
    for(unsigned char i = inst_count()-1; i>idx; i--) {
        audioChannel inst = instruments.back();
        instruments.pop_back();
        erase_instruments.push_back(inst);
    }
    instruments.pop_back();
    while(!erase_instruments.empty()) {
        audioChannel inst = erase_instruments.back();
        erase_instruments.pop_back();
        instruments.push_back(inst);
    }
}
audioChannel* instrumentStorage::at(unsigned char idx) {
    return &instruments.at(idx);
}
