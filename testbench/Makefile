# Makefile for sensord
#Some compiler stuff and flags
CFLAGS += -g -Wall -fpermissive
EXECUTABLE = sensord_mock
_OBJ = sensord_mock.o nmea_checksum.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))
OBJ_CAL = $(patsubst %,$(ODIR)/%,$(_OBJ_CAL))
# LIBS = -lpulse -lm -lpthread
ODIR = obj
BINDIR = /opt/bin/

#targets

$(ODIR)/%.o: %.c
	mkdir -p $(ODIR)
	$(CXX) -c -o $@ $< $(CFLAGS)

all: sensord_mock
	
sensord_mock: $(OBJ)
	$(CXX) -g -o $@ $^ $(LIBS)
	
install: sensord_mock
	install -D variod $(BINDIR)/$(EXECUTABLE)
	
clean:
	rm -f $(ODIR)/*.o *~ core $(EXECUTABLE)

.PHONY: clean all doc	
