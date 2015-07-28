# Makefile for sensord
#Some compiler stuff and flags
CFLAGS += -g -Wall
EXECUTABLE = varioapp
_OBJ = audiovario.o vario_app.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))
OBJ_CAL = $(patsubst %,$(ODIR)/%,$(_OBJ_CAL))
LIBS = -lasound -lm -lpthread
ODIR = obj
BINDIR = /opt/bin/

#targets

$(ODIR)/%.o: %.c
	mkdir -p $(ODIR)
	$(CXX) -c -o $@ $< $(CFLAGS)

all: varioapp
	
doc: 
	@echo Running doxygen to create documentation
	doxygen
	
varioapp: $(OBJ)
	$(CXX) $(LIBS) -g -o $@ $^
	
install: vario_app
	install -D varioapp $(BINDIR)/$(EXECUTABLE)
	
clean:
	rm -f $(ODIR)/*.o *~ core $(EXECUTABLE)
	rm -fr doc

.PHONY: clean all doc	
