/*
  Copyright (C) 2024 Chase Taylor

  This file is part of chTRACKER at path ./src/timer/timer.cxx

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

#include "timer.hxx"
#include <string>
#include <stdexcept>

void timerHandler::addTimer(std::string name, unsigned int inXTicks) {
    for(unsigned int i = 0; i < timers.size(); i++) {
        if(timers.at(i).name == name) {
            throw std::logic_error(const_cast<char *>("timerHandler::addTimer : timer already exists"));
        }
    }
    timers.push_back({ name, inXTicks, inXTicks, timerStatus::ticking });
}

void timerHandler::tick() {
    for(unsigned int i = 0; i < timers.size(); i++) {
        timer &t = timers.at(i);
        if(t.status==timerStatus::complete) continue;
        if(t.ticksLeft > 0) t.ticksLeft--;
        if(t.ticksLeft==0) t.status = timerStatus::complete;
    }
}

bool timerHandler::isComplete(std::string name) {
    for(unsigned int i = 0; i < timers.size(); i++) {
        if(timers.at(i).name == name) {
            return timers.at(i).status == timerStatus::complete;
        }
    }
    throw std::logic_error(const_cast<char *>("timerHandler::isComplete : timer does not exist"));
}

void timerHandler::resetTimer(std::string name, unsigned int inXTicks) {
    for(unsigned int i = 0; i < timers.size(); i++) {
        if(timers.at(i).name == name) {
            timers.at(i).status = timerStatus::ticking;
            timers.at(i).ticksLeft = inXTicks==0 ? timers.at(i).staringTicksLeft : inXTicks;
            return;
        }
    }
    throw std::logic_error(const_cast<char *>("timerHandler::resetTimer : timer does not exist"));
}

bool timerHandler::hasTimer(std::string name) {
    for(unsigned int i = 0; i < timers.size(); i++) {
        if(timers.at(i).name == name) {
            return true;
        }
    }
    return false;
}

void timerHandler::removeTimer(std::string name) {
    for(unsigned int i = 0; i < timers.size(); i++) {
        if(timers.at(i).name == name) {   
            for(unsigned char j = timers.size()-1; j>i; j--) {
                timer t = timers.back();
                timers.pop_back();
                erase_timers.push_back(t);
            }
            timers.pop_back();
            while(!erase_timers.empty()) {
                timer t = erase_timers.back();
                erase_timers.pop_back();
                timers.push_back(t);
            }
            return;
        }
    }
    throw std::logic_error(const_cast<char *>("timerHandler::removeTimer : timer does not exist"));
}