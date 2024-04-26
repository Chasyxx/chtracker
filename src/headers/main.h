/*
  Copyright (C) 2024 Chase Taylor

  This file is part of chTRACKER at path ./src/headers/main.hxx
  This is a general header file which is included in any file which needs basic
  struct, enum and version information.

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

#ifndef CHTRACKER_MAIN_H
#define CHTRACKER_MAIN_H

#ifdef __cplusplus
#define MAIN_H_CONST constexpr
#else
#define MAIN_H_CONST const
#endif

/**************************************
 *                                    *
 *            MACRO SECTION           *
 *   All #define DIRECTIVES GO HERE   *
 *                                    *
 **************************************/

#define AUDIO_SAMPLE_COUNT 512

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

/***********************************************
 *                                             *
 *           ENUM AND SCRUCT SECTION           *
 *   All enum AND struct DEFINITIONS GO HERE   *
 *                                             *
 ***********************************************/

#ifdef __cplusplus

enum class GlobalMenus {
  main_menu,

  help_menu,
  order_menu,
  pattern_menu,
  instrument_menu,
  order_management_menu,
  options_menu,
  file_menu,
  save_file_menu,
  render_menu,
  quit_connfirmation_menu
};

struct Selection {
  unsigned int x = 0;
  unsigned int y = 0;
};

struct CursorPos {
  unsigned int x = 0;
  unsigned int y = 0;
  unsigned char subMenu = 0;
  struct Selection selection;
};

#endif

/*************************************
 *                                   *
 *           CONST SECTION           *
 *   All MAIN_H_CONST VARIABLES GO HERE   *
 *                                   *
 *************************************/

MAIN_H_CONST unsigned char global_majorVersion /**/ = 0x00;
MAIN_H_CONST unsigned char global_minorVersion /**/ = 0x03;
MAIN_H_CONST unsigned char global_patchVersion /**/ = 0x00;
MAIN_H_CONST unsigned char global_prereleaseVersion = 0x00;

MAIN_H_CONST unsigned char patternMenu_instrumentCollumnWidth[] = {3,  6,  12,
                                                                18, 24, 30};
MAIN_H_CONST unsigned char patternMenu_instrumentVariableCount[] = {2,  4,  9,
                                                                 14, 19, 24};

#ifdef MAIN_H_CONST
#undef MAIN_H_CONST
#endif

#endif // ifndef CHTRACKER_MAIN_H