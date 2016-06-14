WACOMTOOLSRC = wacom_flash.c
PROGNAME = wacom_flash
STATIC_BUILD ?= n
ifeq ($(STATIC_BUILD),y)
	LDFLAGS += -static
endif

all:
	$(CC) $(WACOMTOOLSRC) $(WACOMTOOLOBJ) $(CCFLAGS) $(LDFLAGS) -o $(PROGNAME)

clean:
	rm -f $(WACOMTOOLOBJ) $(PROGNAME)
