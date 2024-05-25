/*
  Copyright (C) 2024 Chase Taylor

  This file is part of chTRACKER at path ./src/headers/log.hxx
  This is a header file; For the non-header file see path
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

#include <format>
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
template <typename... Args>
void log(int severity, std::string &format, Args &&...args) {
  std::string msg = std::vformat(format, std::make_format_args(args...));
  if (level <= severity) {
    logs.push_back(
        {.msg = msg, .severity = static_cast<char>(severity), .printed = true});
    std::cerr << severityStrings[severity] << " " << msg << std::endl;
  } else
    logs.push_back({.msg = msg,
                    .severity = static_cast<char>(severity),
                    .printed = false});
}
template <typename... Args> void debug(std::string format, Args &&...args) {
  log(0, format, args...);
}
template <typename... Args> void notice(std::string format, Args &&...args) {
  log(1, format, args...);
}
template <typename... Args> void warning(std::string format, Args &&...args) {
  log(2, format, args...);
}
template <typename... Args> void error(std::string format, Args &&...args) {
  log(3, format, args...);
}
template <typename... Args> void critical(std::string format, Args &&...args) {
  log(4, format, args...);
}
} // namespace log
} // namespace cmd

#endif