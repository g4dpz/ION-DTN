# Makefile.am for LTP KISS CLA
# LTP KISS Convergence Layer Adapter

# Compiler flags
kisscflags = -I$(srcdir)/ltp/kiss -I$(srcdir)/ltp/library -I$(srcdir)/ltp/include

# Programs to build
kissbin = \
	ltpkissclo \
	ltpkisscli

# Libraries (none for now, may add libkiss.la later)
kisslib =

# Public headers (none for now, using ltpkiss.h as noinst)
kissinclude =

# Private headers
kissnoinst = \
	ltp/kiss/ltpkiss.h \
	ltp/kiss/ltpkissP.h

# Extra files for distribution
kissextra = \
	ltp/kiss/README.txt \
	ltp/kiss/doc/pod1/ltpkissclo.pod \
	ltp/kiss/doc/pod1/ltpkisscli.pod

# Man pages (when documentation is complete)
kissmans =

# --- Convergence Layer Output (CLO) --- #

ltpkissclo_SOURCES = \
	ltp/kiss/ltpkissclo.c \
	ltp/kiss/kiss.c \
	ltp/kiss/serial.c \
	ltp/kiss/kissconfig.c \
	ltp/kiss/ax25frame.c

ltpkissclo_LDADD = libltp.la libici.la $(PTHREAD_LIBS)
ltpkissclo_CFLAGS = $(kisscflags) $(AM_CFLAGS)

# --- Convergence Layer Input (CLI) --- #

ltpkisscli_SOURCES = \
	ltp/kiss/ltpkisscli.c \
	ltp/kiss/kiss.c \
	ltp/kiss/serial.c \
	ltp/kiss/kissconfig.c \
	ltp/kiss/ax25frame.c

ltpkisscli_LDADD = libltp.la libici.la $(PTHREAD_LIBS)
ltpkisscli_CFLAGS = $(kisscflags) $(AM_CFLAGS)

# Clean targets
kissclean-local:
	rm -f $(top_builddir)/ltp/kiss/doc/*.1

# Man page generation (pod2man)
kissPODM1 = pod2man -s 1 -c "LTP KISS executables"

$(top_builddir)/ltp/kiss/doc/%.1: ltp/kiss/doc/pod1/%.pod
	$(kissPODM1) $< $@
