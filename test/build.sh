#!/bin/sh
# Build both backends of the JNetHack 3.4.3 SDL port from a clean tree.
#
#   ./test/build.sh          # everything, including the Windows cross build
#   ./test/build.sh tty      # src/jnethack.tty only
#   ./test/build.sh sdl      # src/jnethack.sdl only
#   ./test/build.sh win      # src/jnethack.exe only (MinGW-w64 cross)
#
# Produces:
#   src/jnethack.tty   stock termcap backend (the screen-comparison reference)
#   src/jnethack.sdl   win/tty/sdlterm.c backend
#   src/jnethack.exe   the same backend cross-compiled for Windows
#   dat/*.lev etc.     shared by the two host binaries; they agree on
#                      VERSION_FEATURES because SDL_GRAPHICS is not one of
#                      makedefs' feature bits
#   datwin/dat/*.lev   the same data rebuilt for jnethack.exe, which cannot
#                      read dat/ -- see the note atop sys/unix/Makefile.dat
#
# The Windows phase runs last.  util/makedefs and util/lev_comp link objects
# out of src/, so the whole of src/*.o and util/*.o belongs to one target at
# a time, and the host phases have to be finished with them first.
set -e

cd "$(dirname "$0")/.."
what=${1:-all}

# The stock Makefiles live under sys/unix.  sys/unix/setup.sh keys off the
# current directory and drops them in the wrong place when run from the top
# of the tree, so copy them by hand.
setup_makefiles() {
    cp -f sys/unix/Makefile.top ./Makefile
    cp -f sys/unix/Makefile.dat dat/Makefile
    cp -f sys/unix/Makefile.doc doc/Makefile
    cp -f sys/unix/Makefile.src src/Makefile
    cp -f sys/unix/Makefile.utl util/Makefile
}

# The Windows data files cannot be shared with the host ones -- see the note
# at the top of sys/unix/Makefile.dat -- so they are built from the same
# sources in a tree of their own.
#
# That tree has to be shaped like the real one.  util/makedefs writes its
# outputs through fixed "../dat/%s" and "../include/%s" templates
# (util/makedefs.c:113-125) rather than to the current directory, so the
# staging directory is datwin/dat and datwin/ gets symlinks back to the
# real util, include, src and sys.  Running make in datwin/dat then makes
# "../dat" mean datwin/dat itself, and "../util" the real utilities.
stage_datwin() {
    rm -rf datwin
    mkdir -p datwin/dat
    ln -s ../util datwin/util
    ln -s ../include datwin/include
    ln -s ../src datwin/src
    ln -s ../sys datwin/sys        # for sys/winnt/porthelp
    cp -f dat/* datwin/dat/ 2>/dev/null || true
    rm -f datwin/dat/Makefile datwin/dat/*.lev \
          datwin/dat/spec_levs datwin/dat/quest_levs \
          datwin/dat/data datwin/dat/jrumors datwin/dat/quest.dat \
          datwin/dat/joracles datwin/dat/options datwin/dat/ttyoptions \
          datwin/dat/dungeon datwin/dat/dungeon.pdf datwin/dat/nhdat
    cp -f sys/unix/Makefile.dat datwin/dat/Makefile
}

# yacc/lex are not needed: the tree ships pre-generated parsers under
# sys/share.  They are copied in rather than regenerated.
seed_parsers() {
    cp -f sys/share/lev_yacc.c sys/share/lev_lex.c \
	  sys/share/dgn_yacc.c sys/share/dgn_lex.c util/
    cp -f sys/share/lev_comp.h sys/share/dgn_comp.h include/
    touch util/lev_yacc.c util/lev_lex.c util/dgn_yacc.c util/dgn_lex.c
    touch include/lev_comp.h include/dgn_comp.h
}

setup_makefiles

# The data files carry a VERSION_FEATURES word that lev_comp and dgn_comp
# stamp in from include/date.h.  Changing a config.h feature invalidates
# them, and the pre-generated parsers hide the dependency, so the utilities
# are rebuilt from scratch every time.
rm -f util/*.o util/makedefs util/lev_comp util/dgn_comp util/recover util/dlb
rm -f include/date.h
# ../util/makedefs links src/monst.o and src/objects.o.  A previous Windows
# phase leaves cross-compiled ones there, which the host linker rejects.
rm -f src/*.o
seed_parsers

echo "=== building utilities ==="
make -C util

if [ "$what" = all ] || [ "$what" = tty ]; then
    echo "=== building tty backend ==="
    rm -f src/*.o src/jnethack
    make -C src
    mv src/jnethack src/jnethack.tty
fi

if [ "$what" = all ] || [ "$what" = sdl ]; then
    echo "=== building SDL backend ==="
    rm -f src/*.o src/jnethack
    make -C src SDLGRAPH=1
    mv src/jnethack src/jnethack.sdl
fi

echo "=== building data files ==="
make -C dat spotless >/dev/null 2>&1 || true
make -C dat

if [ "$what" = all ] || [ "$what" = win ]; then
    # Last on purpose: from here on src/*.o and util/*.o are Windows
    # objects, which the host makedefs and lev_comp cannot be linked from.
    echo "=== building Windows (MinGW-w64) SDL backend ==="
    rm -f src/*.o util/*.o src/jnethack.exe
    rm -f util/makedefs.exe util/lev_comp.exe util/dgn_comp.exe
    # include/date.h carries VERSION_FEATURES and the struct sizes, so it
    # has to come from a makedefs built for the target before src is built.
    rm -f include/date.h
    make -C src MINGW=1

    echo "=== building Windows data files ==="
    stage_datwin
    make -C datwin/dat MINGW=1

    # makedefs -v writes include/date.h and dat/options through the same
    # fixed templates, so both now describe the Windows build.  Put the
    # host's back; util/makedefs is still the one the host phase built.
    rm -f include/date.h
    ( cd util && ./makedefs -v )
fi

rm -f src/*.o
ls -l src/jnethack.tty src/jnethack.sdl src/jnethack.exe 2>/dev/null || true
