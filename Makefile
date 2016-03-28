#
# Makefile for i7z, GPL v2, License in COPYING
#

#makefile updated from patch by anestling

#explicitly disable two scheduling flags as they cause segfaults, two more seem to crash the GUI version so putting them
#here 
#CFLAGS_FOR_AVOIDING_SEG_FAULT = -ifno-schedule-insns2  -fno-schedule-insns -fno-inline-small-functions -fno-caller-saves
#CFLAGS ?= -O3
#CFLAGS += $(CFLAGS_FOR_AVOIDING_SEG_FAULT) -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -DBUILD_MAIN -Wimplicit-function-declaration

LBITS := $(shell getconf LONG_BIT)
#ifeq ($(LBITS),64)
#   CFLAGS += -Dx64_BIT
#else
#   CFLAGS += -Dx86
#endif

CC       ?= gcc

LIBS  += -lm
INCLUDEFLAGS = 

BIN	= i7z
# PERFMON-BIN = perfmon-i7z #future version to include performance monitor, either standalone or integrated
SRC	= i7z_short.c 
OBJ	= $(SRC:.c=.o)

prefix ?= /usr
sbindir = $(prefix)/sbin/
docdir = $(prefix)/share/doc/$(BIN)/
mandir ?= $(prefix)/share/man/

all: clean test_exist

message:
	@echo "If the compilation complains about not finding ncurses.h, install ncurses (libncurses5-dev on ubuntu/debian)"

bin: message $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BIN) $(OBJ) $(LIBS)

#http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=644728 for -ltinfo on debian
static-bin: message $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BIN) $(OBJ) -static-libgcc -DNCURSES_STATIC -static -lpthread -lncurses -lrt -lm -ltinfo

# perfmon-bin: message $(OBJ)
# 	$(CC) $(CFLAGS) $(LDFLAGS) -o $(PERFMON-BIN) perfmon-i7z.c helper_functions.c $(LIBS)

test_exist: bin
	@test -f i7z && echo 'Succeeded, now run sudo ./i7z' || echo 'Compilation failed'

clean:
	rm -f *.o $(BIN)

distclean: clean
	rm -f *~ \#*

install:  $(BIN)
	install -D -m 755 $(BIN) $(DESTDIR)$(sbindir)$(BIN)
