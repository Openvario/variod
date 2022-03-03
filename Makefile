# Makefile for sensord
#Some compiler stuff and flags
CFLAGS += -std=c11 -D_GNU_SOURCE
CFLAGS += -g -Wall -Wextra -Wmissing-prototypes
EXECUTABLE = variod
_OBJ = audiovario.o variod.o cmdline_parser.o configfile_parser.o nmea_parser.o stf.o utils.o log.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))
OBJ_CAL = $(patsubst %,$(ODIR)/%,$(_OBJ_CAL))
LIBS = -lpulse -lm -lpthread
ODIR = obj
BINDIR = /opt/bin/
GIT_VERSION := $(shell git describe --abbrev=4 --dirty --always --tags)

#targets

$(ODIR)/%.o: %.c
	mkdir -p $(ODIR)
	$(CC) -DVERSION_GIT=\"$(GIT_VERSION)\" -c -o $@ $< $(CFLAGS)

all: variod

doc:
	@echo Running doxygen to create documentation
	doxygen

version.h:
	@echo Git version $(GIT_VERSION)

variod: $(OBJ)
	$(CC) -g -o $@ $^ $(LIBS)

install: variod
	install -D variod $(BINDIR)/$(EXECUTABLE)

clean:
	rm -f $(ODIR)/*.o *~ core $(EXECUTABLE)
	rm -fr doc

.PHONY: clean all doc
