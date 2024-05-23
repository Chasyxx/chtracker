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

#include <iostream>
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
std::vector<struct log> logs;
void log(int loglevel, int severity, std::string msg) {
  if (loglevel <= severity) {
    logs.push_back({.msg = msg, .severity = static_cast<char>(severity), .printed = true});
    std::cerr << severityStrings[severity] << " " << msg << std::endl;
  } else
    logs.push_back({.msg = msg, .severity = static_cast<char>(severity), .printed = false});
}
void debug(int loglevel, std::string msg) /*****/ { log(loglevel, 0, msg); }
void notice(int loglevel, std::string msg) /****/ { log(loglevel, 1, msg); }
void warning(int loglevel, std::string msg) /***/ { log(loglevel, 2, msg); }
void error(int loglevel, std::string msg) /*****/ { log(loglevel, 3, msg); }
void critical(std::string msg) /**/ { log(4, 4, msg); }
} // namespace log
} // namespace cmd
