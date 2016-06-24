WACOMTOOLSRC = wacom_flash.c
PROGNAME = wacom_flash
STATIC_BUILD ?= n
ifeq ($(STATIC_BUILD),y)
	LDFLAGS += -static
endif

all:
	$(CC) $(CFLAGS) $(LDFLAGS) $(WACOMTOOLSRC) $(WACOMTOOLOBJ) -o $(PROGNAME)

clean:
	rm -f $(WACOMTOOLOBJ) $(PROGNAME)
