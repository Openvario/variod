# Makefile for sensord
#Some compiler stuff and flags
CFLAGS += -g -Wall -fpermissive
EXECUTABLE = sensord_mock
_OBJ = nmea_checksum.o sensord_mock.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))
OBJ_CAL = $(patsubst %,$(ODIR)/%,$(_OBJ_CAL))
LIBS = -lpulse -lm -lpthread
ODIR = obj
BINDIR = /opt/bin/

#targets

$(ODIR)/%.o: %.c
	mkdir -p $(ODIR)
	$(CXX) -c -o $@ $< $(CFLAGS)

all: sensord_mock my_netcat
	
sensord_mock: $(OBJ)
	$(CXX) -g -o $@ $^ $(LIBS)
	
my_netcat: my_netcat.c
	$(CXX) -g -o $@ $^ $(LIBS)

install: sensord_mock
	install -D $(EXECUTABLE) $(BINDIR)/$(EXECUTABLE)
	
clean:
	rm -f $(ODIR)/*.o my_netcat.o my_netcat *~ core $(EXECUTABLE)

.PHONY: clean all
