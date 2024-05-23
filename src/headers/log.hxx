/*
  Copyright (C) 2024 Chase Taylor

  This file is part of chTRACKER at path ./src/headers/log.hxx
  This is a declaration file; For implementation see path
  ./src/log/log.c

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

#ifndef LIBRARY_LOG
#define LIBRARY_LOG

/*
  Copyright (C) 2024 Chase Taylor

  This file is part of chTRACKER at path ./src/log/log.cxx

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

#include <string>
#include <vector>

static constexpr char const *severityStrings[] = {
    "\x1b[1m[debug]\x1b[0m", "\x1b[1;34m[note ]\x1b[0m",
    "\x1b[1;33m[ WARN]\x1b[0m", "\x1b[1;31m[ERROR]\x1b[0m",
    "\x1b[1;35m[CRIT!]\x1b[0m"};

struct log {
  std::string msg;
  char severity;
  bool printed;
};

namespace cmd {
namespace log {
/**
 * Log level.
 * Anything above 4 prints nothing.
 * 
 * 0 - all logs (debug, notice, warn, error, critical)
 * 
 * 1 - Notices (notice, warn, error, critical)
 * 
 * 2 - Warnings (warn and up)
 * 
 * 3 - Errors (error and critical)
 * 
 * 4 - Critical
 */
extern int level;
extern std::vector<struct log> logs;
void debug(std::string msg);
void notice(std::string msg);
void warning(std::string msg);
void error(std::string msg);
void critical(std::string msg);
} // namespace log
} // namespace cmd

#endif