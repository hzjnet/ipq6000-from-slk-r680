CROSS-COMPILE =
CC := $(CROSS-COMPILE)gcc

# Stolen from Linux build system
comma = ,
try-run = $(shell set -e; ($(1)) >/dev/null 2>&1 && echo "$(2)" || echo "$(3)")
cc-option = $(call try-run, $(CC) $(1) -c -xc /dev/null -o /dev/null,$(1),$(2))

CFLAGS ?= -s
WFLAGS := -Wall \
	$(call cc-option,-Wextra) \
	$(call cc-option,-Wwrite-strings) \
	$(call cc-option,-Wno-sign-compare) \
	$(call cc-option,-Wno-unused-function)
CFLAGS += $(WFLAGS)

simcom-cm_SOURCES = QmiWwanCM.c GobiNetCM.c main.c MPQMUX.c QMIThread.c util.c udhcpc.c usbmisc.c mbim-cli.c
qmi-proxy_SOURCES = qmi-proxy.c

release: clean
	$(CC) $(CFLAGS) ${simcom-cm_SOURCES} -o simcom-cm -lpthread -ldl

qmi-proxy:
	$(CC) $(CFLAGS) ${qmi-proxy_SOURCES} -o qmi-proxy -lpthread -ldl

cli:
	$(CC) $(CFLAGS) mbim-cm.c -o cli -lpthread -ldl

clean:
	@-rm -rf simcom-cm qmi-proxy cli

