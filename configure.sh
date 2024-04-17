#!/bin/sh

WARNINGS=0
COUNT_WARNINGS=0

on() { ! [ -v $1 ]; }

# Default values
WERROR_ENABLED=1
AUTOADD_SDL=1
DRY_RUN=0
if on $CFLAGS; then
	WERROR_ENABLED=0
else
	CFLAGS="-O2 -Wall -Wextra -I$PWD/src/headers"
fi

on $BINDIRS && BINDIRS="$BINDIRS "
BINDIRS="$BINDIRS/usr/local/bin /usr/bin /bin"

on $CC || CC=gcc
on $CXX || CXX=g++
on $CCLD || CCLD=ld

### OPTIONAL DEPS TRACKING
	TRACK_PERL=0
	TRACK_INSTALL=0
### END OPTIONAL DEPS TRACKING

print_usage() {
	echo "Usage: $0 [-n|--dry] [-h|--help] [--disable-werror] [--disable-sdl] [--clean]"
	# echo "Parameters needed for long options are needed for short options too."
	echo ""
	echo "Usable flags:"
	echo "-h --help        : Show this list"
	echo ""
	echo "--disable-werror : Removes the -Werror from the compiler flags. Implicit with a"
	echo "                   custom CFLAGS variable."
	echo ""
	echo "--disable-sdl    : Using this disables all SDL checks. It won't check for"
	echo "                   sdl2-config, it won't check SDL's include and library paths,"
	echo "                   and it won't add SDL's cflags and libs. Set custom paths with"
	echo "                   the CFLAGS and LIBS enviornment variables if you use this"
	echo "                   flag."
	echo ""
	echo "--clean          : Runs \`make clean\`, and then removes all makefiles. A total"
	echo "                   clean of the source tree."
	echo ""
	echo ""
	echo "Usable enviornment variables:"
	echo "CXX    : C++ compiler. Defaults to g++"
	echo ""
	echo "CC     : C compiler. Defaults to gcc"
	echo ""
	echo "CCLD   : Linker. Defaults to ld"
	echo ""
	echo "CFLAGS : Custom cflags. Implicitly concatenated with SDL cflags unless"
	echo "         --disable-sdl is used."
	echo ""
	echo "LIBS   : Custom libss. Implicitly concatenated with SDL libs unless"
	echo "         --disable-sdl is used."
	#    "--------------------------------------------------------------------------------"
}

availible() { command -v $1 1> /dev/null; }
exists() { stat $1 1> /dev/null 2> /dev/null; }
pass() { echo -e $2 "\e[32m[PASS!]\e[0m $1" 1>&2; }
notice() { echo -e $2 "\e[34m[INFO ]\e[0m $1" 1>&2; }
warning() { echo -e $2 "\e[33m[ WARN]\e[0m $1" 1>&2; [ $COUNT_WARNINGS -eq 1 ] && WARNINGS=`expr $WARNINGS + 1`;}
error() { echo -e $2 "\e[31m[ERROR]\e[0m $1" 1>&2; exit 1; }

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
		-h|--help)
			print_usage
			exit 0
			;;
		-n|--dry)
			DRY_RUN=1
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
		pass "$1 found"
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
want 		expr 		&& COUNT_WARNINGS=1
prefer 		$CCLD
want 		install 	&& TRACK_INSTALL=1
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
				error "SDL2 include path $path does not exist"
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
				error "SDL2 library path $path does not exist"
			fi
		done
	fi

	CFLAGS="$CFLAGS $(sdl2-config --cflags)"
	LIBS="$LIBS $(sdl2-config --libs)"
fi
notice "Final cflags are $CFLAGS"
notice "Final libs are $LIBS"

[ $DRY_RUN -eq 1 ] && exit 0;

checkwarnings
notice "Generating Makefiles"

notice " - ./src/order"
cat > src/order/Makefile << EOS
CC=$CC
CXX=$CXX
CFLAGS=$CFLAGS
LIBS=$LIBS
CCLD=$CCLD

all: ../order.oxx

../order.oxx: order.cxx
	\$(CXX) \$(CFLAGS) -c -o \$@ \$<
EOS

notice " - ./src/timer"
cat > src/timer/Makefile << EOS
CC=$CC
CXX=$CXX
CFLAGS=$CFLAGS
LIBS=$LIBS
CCLD=$CCLD

all: ../timer.oxx

../timer.oxx: timer.cxx
	\$(CXX) \$(CFLAGS) -c -o \$@ \$<
EOS

notice " - ./src/channel"
cat > src/channel/Makefile << EOS
CC=$CC
CXX=$CXX
CFLAGS=$CFLAGS
LIBS=$LIBS
CCLD=$CCLD

all: ../channel.oxx

../channel.oxx: channel.cxx
	\$(CXX) \$(CFLAGS) -c -o \$@ \$<
EOS

notice " - ./src/visual"
cat > src/visual/Makefile << EOS
CC=$CC
CXX=$CXX
CFLAGS=$CFLAGS
LIBS=$LIBS
CCLD=$CCLD

all: ../visual.o

../visual.o: visual.c font.i
	\$(CC) \$(CFLAGS) -c -o \$@ \$<

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
LIBS=$LIBS
CCLD=$CCLD
CLEAN=rm -fv

all: ../chtracker

clean:
	\$(CLEAN) ./*.o
	\$(CLEAN) ./*/*.o
	\$(CLEAN) ./*.oxx
	\$(CLEAN) ./visual/font.i
	\$(CLEAN) ../chtracker
	\$(CLEAN) ../chtracker.exe

../chtracker: timer.oxx order.oxx channel.oxx visual.o chtracker.oxx
	\$(CXX) \$(LIBS) -o \$@ \$^

visual.o: visual/font.i visual/visual.c
	@\$(MAKE) -C visual

visual/font.i: visual/font.pl visual/font.charset
	@\$(MAKE) -C visual font.i

order.oxx: order/order.cxx
	@\$(MAKE) -C order

channel.oxx: channel/channel.cxx
	@\$(MAKE) -C channel

timer.oxx: timer/timer.cxx
	@\$(MAKE) -C timer

%.out: %.o
	\$(CC) \$(LIBS) -o \$@ \$<

%.out: %.oxx
	\$(CXX) \$(LIBS) -o \$@ \$<

%.o: %.c
	\$(CC) \$(CFLAGS) -c -o \$@ \$<

%.oxx: %.cxx
	\$(CXX) \$(CFLAGS) -c -o \$@ \$<
EOS

notice " - ."
cat > Makefile << EOS
all:
	@\$(MAKE) -C src

clean:
	@\$(MAKE) -C src clean

font:
	@\$(MAKE) -C src/visual font.i
EOS

notice "Done creating makefiles"
