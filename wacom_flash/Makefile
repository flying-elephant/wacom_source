CXX ?= gcc
CPPFLAGS += -I../include -I./include
CXXFLAGS += -Wall
WACOMTOOLSRC = wacom_flash.c
WACOMTOOLOBJ = $WACOMTOOLSRC:.c=.o)
PROGNAME = wacom_flash
STATIC_BUILD ?= y
ifeq ($(STATIC_BUILD),y)
	LDFLAGS += -static
endif

all: $(PROGNAME)
	$(PROGNAME): $(WACOMTOOLOBJ)
	$(CXX) $(CXXFLAGS) $(WACOMTOOLOBJ) -o $(PROGNAME)
clean:
	rm -f $(WACOMTOOLOBJ) $(PROGNAME)
