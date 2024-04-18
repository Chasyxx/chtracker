# chTRACKER: A fakebit chiptune tracker with the idea of having various base instruments

## Usage
See doc/help.txt: <https://github.com/Chasyxx/chtracker/blob/main/doc/help.txt>. It says hi!

## Compilation
Ensure you're in the source tree (the directory contaaining `src`, not `src` itself).
1. `./configure.sh --clean` - Best to make sure you have a clean source tree.
2. `./connfigure.sh` - This sets everything up for you.
 * If you have MinGW-w64 and want to make a Windows build, try the below command instead. It assumes you cross-compiled SDL statically to `$HOME/.local/sdl2-mingw-w64`.
3. `make` - Time to build the source tree!

Now you have `chtracker` or `chtracker.exe`.
```bash
# Example windows cross compilation command
CFLAGS="-O2 -I$HOME/.local/sdl2-mingw-w64/include/ -Dmain=SDL_main -I$PWD/src/headers -Wall -Wextra -Werror" LIBS="$($HOME/.local/sdl2-mingw-w64/bin/sdl2-config --libs) --static" CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++ ./configure.sh --disable-sdl
```

## If you are using a language server
After configuring, you'll need to make some changes to the language server you're using to ensure you don't get any errors that shouldn't be there:
1. Add the headers folder (run `echo $(pwd)/src/headers` to get a path) to your include path
2. `make font` removes an error in `visual.o` about `font.i` not existing.

**These only influence your language server. The build itaelf is completely handled by the configure script and makefiles.**