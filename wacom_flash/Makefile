WACOMTOOLSRC = read_hex.c wacom_flash_aes.c wacom_flash.c
PROGNAME = wacom_flash
STATIC_BUILD ?= n
ifeq ($(STATIC_BUILD),y)
	LDFLAGS += -static
endif

all: $(PROGNAME)
$(PROGNAME): $(WACOMTOOLSRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(WACOMTOOLSRC) -o $(PROGNAME)

clean:
	rm -f $(WACOMTOOLOBJ) $(PROGNAME)
