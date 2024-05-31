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

#include <vector>
#include "log.hxx"

namespace cmd {
namespace log {
int level = 1;
std::vector<struct log> logs;
} // namespace log
} // namespace cmd
