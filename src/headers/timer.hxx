/*
  Copyright (C) 2024 Chase Taylor

  This file is part of chTRACKER at path ./src/headers/timer.hxx
  This is a declaration file; For implementation see path
  ./src/timer/timer.cxx

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
#ifndef _CHTRACKER_TIMER_HXX
#define _CHTRACKER_TIMER_HXX

#include <string>
#include <vector>

enum class timerStatus {
    ticking,
    complete
};

struct timer {
    std::string name;
    unsigned int ticksLeft;
    unsigned int staringTicksLeft;
    timerStatus status;
};

class timerHandler {
    private:
    std::vector<timer> timers;
    std::vector<timer> erase_timers;
    public:
    void addTimer(std::string name, unsigned int in_x_ticks);
    void tick();
    bool isComplete(std::string name);
    void resetTimer(std::string name, unsigned int inXTicks);
    bool hasTimer(std::string name);
    void removeTimer(std::string name);
};

#endif