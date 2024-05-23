#!/bin/sh

WARNINGS=0
COUNT_WARNINGS=0
command -v expr 2> /dev/null 1> /dev/null && COUNT_WARNINGS=1

on() { [ -n "$1" ]; }

# Default values
WERROR_ENABLED=1
AUTOADD_SDL=1
DRY_RUN=0
AUTODEFINE_CXX=1
ICON=0
if on $CFLAGS; then
	WERROR_ENABLED=0
else
	CFLAGS="-O2 -Wall -Wextra -I$PWD/src/headers"
fi
on $CXXFLAGS && AUTODEFINE_CXX=0
PREFIX="/usr/local"
on $BINDIRS && BINDIRS="$BINDIRS "
BINDIRS="$BINDIRS/usr/local/bin /usr/bin /bin"

on $CC   || CC="gcc -c"
on $CXX  || CXX="g++ -c"
on $CCLD || CCLD=g++
on $RES  || RES=windres

### OPTIONAL DEPS TRACKING
	TRACK_PERL=0
### END OPTIONAL DEPS TRACKING

print_usage() {
	echo "\
Usage: $0 [-n|--dry] [-h|--help] [--disable-werror] [--disable-sdl] [--clean]

Usable flags:
-h --help        : Show this list
--disable-werror : Removes the -Werror from the compiler flags. Implicit with a
                   custom CFLAGS variable.

--disable-sdl    : Using this disables all SDL checks. It won't check for
                   sdl2-config, it won't check SDL's include and library paths,
                   and it won't add SDL's cflags and libs. Set custom paths with
                   the CFLAGS and LIBS enviornment variables if you use this
                   flag.

--clean          : Runs \`make clean\`, and removes all makefiles. A complete
                   cleanup of the source tree.

--manual-cxx     : Disables automatically setting CXXFLAGS to CFLAGS with C++
                   parameters. Implicit if CXXFLAGS is explicitly set.
                    

Usable enviornment variables:
CXX      : C++ compiler. Defaults to \"g++ -c\"
CXX      : C   compiler. Defaults to \"gcc -c\"
CCLD     : Compiler linker. Defaults to \"g++\"
RES      : Resource generator (currently only used for cross compilation.)
           Defaults to \"windres\"
CFLAGS   : Custom C flags. SDL cflags are added automatically unless
           --disable-sdl is used.
CXXFLAGS : C++ version of CFLAGS.
LIBS     : Custom libs. SDL libs are added unless --disable-sdl is used.\
"
}

availible() { command -v $1 1> /dev/null; }
exists() { test -e $1; }
pass() { printf $2 "\e[32m[PASS!]\e[0m $1\n" 1>&2; }
notice() { printf $2 "\e[34m[INFO ]\e[0m $1\n" 1>&2; }
warning() { printf $2 "\e[33m[ WARN]\e[0m $1\n" 1>&2; [ $COUNT_WARNINGS -eq 1 ] && WARNINGS=`expr $WARNINGS + 1`;}
error() { printf $2 "\e[31m[ERROR]\e[0m $1\n" 1>&2; exit 1; }

while [ $# -gt 0 ]; do
	case $1 in
		--clean)
			make clean
			rm -v ./Makefile ./src/Makefile ./src/{visual,order,channel,timer}/Makefile
			exit 0
			;;
		--disable-werror)
			WERROR_ENABLED=0
			notice "Werror disabled"
			;;
		--disable-sdl)
			AUTOADD_SDL=0
			on $LIBS || warning "--disable-sdl used but \$LIBS was not set (linking may fail)"
			;;
		--prefix=*)
			PREFIX="${1#*=}"
			notice "Prefix set to $PREFIX"
			;;
		--manual-cxx)
			AUTODEFINE_CXX=0
			;;
		-h|--help)
			print_usage
			exit 0
			;;
		-n|--dry)
			DRY_RUN=1
			;;
		-i|--icon)
			ICON=1
			;;
		*)
			print_usage
			error "Unknown option $1"
			exit 1
			;;
	esac
	shift
done

if [ $WERROR_ENABLED -eq 1 ]; then
	CFLAGS="$CFLAGS -Werror"
fi

checkwarnings() {
	if availible expr; then
		if [ $WARNINGS -ne 0 ] ; then
			warning "$WARNINGS warning(s)";
			WARNINGS=`expr $WARNINGS - 1`
		else
			notice "No warnings"
		fi
	fi
}

require() {
	if availible $1; then
		pass "$1 found"
	else
		error "You do not have $1"
	fi
}

want() {
	notice "Looking for $1..."
	if availible $1; then
		pass "$1 found"
		return 0
	else
		warning "You do not have $1"
		return 1
	fi
}

prefer() {
	notice "Looking for $1..."
	if availible $1; then
		notice "$1 found"
		return 0
	else
		warning "You do not have $1"
		return 1
	fi
}

lookfor() {
	notice "Looking for $1..."
	if availible $1; then
		notice "$1 found"
		return 0
	else
		notice "You do not have $1"
		return 1
	fi
}

command echo > /dev/null || error "You do not have \`command\`, which is necessary for depchecking"

trap checkwarnings EXIT

if ! availible "["; then
	error "You do not have ["
fi

if ! availible stat; then
	error "You do not have stat"
fi

require		$CXX
require 	$CC
require		$CCLD
if [ $ICON -eq 1 ]; then
	require $RES
else
	lookfor $RES
fi
want 		install     || notice "\`install\` is needed for \`make install\`"
lookfor 	perl 		&& TRACK_PERL=1

if [ $AUTOADD_SDL -eq 1 ]; then
	require 	sdl2-config

	if [ $TRACK_PERL -eq 1 ]; then
		notice "Checking SDL lib and include locations..."
		SDL2_CFLAGS_PATHS=$(sdl2-config --cflags | perl ./config-helpers/sdl-path.pl)
		SDL2_LIBS_PATHS=$(sdl2-config --libs | perl ./config-helpers/sdl-path.pl)
		for path in $SDL2_CFLAGS_PATHS; do
			if exists $path; then
				pass "Path $path exists"
				if exists $path/SDL_*; then
					notice  "Path $path has SDL files"
					if exists $path/SDL.h; then
						notice  "Path $path/SDL.h exists"
					else
						warning "Path $path/SDL.h does not exist"
					fi
				else
					warning "Path $path does not have SDL files"
				fi
			else
				warning "SDL2 include path $path does not exist"
			fi
		done
		for path in $SDL2_LIBS_PATHS; do
			if exists $path; then
				pass "Path $path exists"
				if exists $path/SDL2; then
					notice "Path $path/SDL2 exists"
				else
					if exists $path/libsdl2; then
						notice "Path $path/libsdl2 exists"
					else
						if exists $path/libSDL2; then
							notice "Path $path/libSDL2 exists"
						else
							if exists $path/libSDL2.so; then
								notice "Path $path/libSDL2.so exists"
							else
								warning "SDL2 library path $path does not contail any SDL-related directories/files"
							fi
						fi
					fi
				fi
			else
				warning "SDL2 library path $path does not exist (Linking could fail)"
			fi
		done
	fi

	CFLAGS="$CFLAGS $(sdl2-config --cflags)"
	LIBS="$LIBS $(sdl2-config --libs)"
fi

BINDIR=$PREFIX/bin
if ! exists $BINDIR; then
	warning "$BINDIR doesn't exist!"
fi

DOCDIR=/usr/share/doc
if ! exists $DOCDIR; then
	warning "$DOCDIR doesn't exist!"
fi

if [ $AUTODEFINE_CXX -eq 1 ]; then
	CXXFLAGS="$CFLAGS -std=c++17"
else if ! on $CXXFLAGS; then
	CXXFLAGS=$CFLAGS
fi
fi

checkwarnings

notice "C   flags: $CFLAGS"
notice "C++ flags: $CXXFLAGS"
notice "lib flags: $LIBS" 
notice "bin dir  : $BINDIR"
notice "doc dir  : $DOCDIR"


[ $DRY_RUN -eq 1 ] && exit 0;

notice "Generating Makefiles"

notice " - ./src/visual"
cat > src/visual/Makefile << EOS
CC=$CC
CFLAGS=$CFLAGS

all: ../visual.o

../visual.o: visual.c font.i
	\$(CC) \$(CFLAGS) -o \$@ \$<

EOS

if [ $TRACK_PERL -eq 1 ]; then
	cat >> src/visual/Makefile << EOS
font.i: font.pl
	perl \$< > \$@
EOS
else
	cat >> src/visual/Makefile << EOS
font.i: ../../backups/font.i
	cp \$< \$@
EOS
fi

notice " - ./src"

cat > src/Makefile << EOS
CC=$CC
CXX=$CXX
CFLAGS=$CFLAGS
CXXFLAGS=$CXXFLAGS
LIBS=$LIBS
CCLD=$CCLD
BINDIR=$BINDIR
DOCDIR=$DOCDIR
RES=$RES
CLEAN=rm -fv

all: ../chtracker

install: ../chtracker ../doc/help.txt
	install -D -m 755 ../chtracker \$(BINDIR)/chtracker
	install -D -m 644 ../doc/help.txt \$(DOCDIR)/chtracker/help.txt

clean:
	\$(CLEAN) ./*.o
	\$(CLEAN) ./*/*.o
	\$(CLEAN) ./*.oxx
	\$(CLEAN) ./visual/font.i
	\$(CLEAN) ../chtracker
	\$(CLEAN) ../chtracker.exe

EOS
if [ $ICON -eq 1 ]; then
	cat >> src/Makefile << ----EOS
../chtracker: timer.oxx order.oxx channel.oxx visual.o resources.o chtracker.oxx
	\$(CCLD) -o \$@ \$^ \$(LIBS)

resources.o: resources.rc
	\$(RES) \$< -o \$@
----EOS
else
	cat >> src/Makefile << ----EOS
../chtracker: timer.oxx order.oxx channel.oxx visual.o chtracker.oxx
	\$(CCLD) -o \$@ \$^ \$(LIBS)
----EOS
fi
cat >> src/Makefile << EOS

visual.o: visual/font.i visual/visual.c
	@\$(MAKE) -C visual

visual/font.i: visual/font.pl visual/font.charset
	@\$(MAKE) -C visual font.i

order.oxx: order.cxx
	\$(CXX) \$(CXXFLAGS) -o \$@ \$<

channel.oxx: channel.cxx
	\$(CXX) \$(CXXFLAGS) -o \$@ \$<

timer.oxx: timer.cxx
	\$(CXX) \$(CXXFLAGS) -o \$@ \$<

chtracker.oxx: chtracker.cxx screenUpdate.cxx onKeydown.cxx
	\$(CXX) \$(CXXFLAGS) -o \$@ \$<

%.out: %.o
	\$(CCLD) \$(LIBS) -o \$@ \$<

%.out: %.oxx
	\$(CCLD) \$(LIBS) -o \$@ \$<

%.o: %.c
	\$(CC) \$(CFLAGS) -o \$@ \$<

%.oxx: %.cxx
	\$(CXX) \$(CXXFLAGS) -o \$@ \$<
EOS

notice " - ."
cat > Makefile << EOS
all:
	@\$(MAKE) -C src

clean:
	@\$(MAKE) -C src clean

install:
	@\$(MAKE) -C src install

font:
	@\$(MAKE) -C src/visual font.i	
EOS

notice "Done creating makefiles"
