# chTRACKER: A fakebit chiptune tracker with the idea of having various base instruments

## Usage
See doc/help.txt: <https://github.com/Chasyxx/chtracker/blob/main/doc/help.txt>. It says hi!

## Dependencies
* [SDL2](https://github.com/libsdl-org/SDL/tree/SDL2) (`sudo apt-get install libsdl2-dev` or `sudo pacman -S sdl2`)
* [libfmt](https://github.com/fmtlib/fmt) (`sudo apt-get install libfmt-dev` or `sudo pacman -S fmt`)

## Compilation
Ensure you're in the source tree (the directory contaaining `src`, not `src` itself).
1. `./configure.sh --clean` - Best to make sure you have a clean source tree.
2. `./configure.sh` - This sets everything up for you.
 * If you have MinGW-w64 and want to make a Windows build, try the below command instead. It assumes you cross-compiled SDL statically to `$HOME/.local/sdl2-mingw-w64`, and libformat to `$HOME/.local/fmt-mingw-w64`. You can change these paths if wanted.
3. `make` - Time to build the source tree!

Now you have `chtracker` or `chtracker.exe`.
```bash
# Example windows cross compilation command
CFLAGS="-O2 -I$HOME/.local/{sdl2,fmt}-mingw-w64/include/ -Dmain=SDL_main -I$PWD/src/headers -Wall -Wextra -Werror" LIBS="$($HOME/.local/sdl2-mingw-w64/bin/sdl2-config --libs) -L$HOME/.local/fmt-mingw-w64/lib/ -lfmt --static" CC="x86_64-w64-mingw32-gcc -c" CXX="x86_64-w64-mingw32-g++ -c" CCLD=x86_64-w64-mingw32-g++ RES=x86_64-w64-mingw32-windres ./configure.sh --disable-sdl--icon
# big command!
```

## If you are using a language server
After configuring, you'll need to do some things to ensure you don't get any errors that shouldn't be there:
1. Add the headers folder (run `echo $(pwd)/src/headers` to get a path) to your include path (*This is just for the language server, the build itself has this done by the configure script*)
2. `make font` removes an error in `visual.o` about `font.i` not existing.

