WACOMTOOLSRC = wacom_flash.c
PROGNAME = wacom_flash
STATIC_BUILD ?= y
ifeq ($(STATIC_BUILD),n)
	LDFLAGS += -static
endif

all:
	$(CC) $(WACOMTOOLSRC) $(WACOMTOOLOBJ) $(CCFLAGS) $(LDFLAGS) -o $(PROGNAME)

clean:
	rm -f $(WACOMTOOLOBJ) $(PROGNAME)
