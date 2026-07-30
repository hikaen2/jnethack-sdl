#!/bin/sh
# Build JNetHack from a clean tree.
#
# Produces:
#   src/jnethack.tty   the termcap backend
#   dat/*.lev etc.     the compiled data files
set -e

cd "$(dirname "$0")/.."

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
seed_parsers

echo "=== building utilities ==="
make -C util

echo "=== building tty backend ==="
rm -f src/*.o src/jnethack
make -C src
mv src/jnethack src/jnethack.tty

echo "=== building data files ==="
make -C dat spotless >/dev/null 2>&1 || true
make -C dat

rm -f src/*.o
ls -l src/jnethack.tty
