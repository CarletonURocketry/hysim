include $(APPDIR)/Make.defs

# Application information for pad server

PROGNAME = $(CONFIG_HYSIM_PAD_SERVER_PROGNAME)
PRIORITY = $(CONFIG_HYSIM_PAD_SERVER_PRIORITY)
STACKSIZE = $(CONFIG_HYSIM_PAD_SERVER_STACKSIZE)
MODULE = $(CONFIG_HYSIM_PAD_SERVER)

# Pad server recipe

MAINSRC = src/pad_server_main.c

CSRCS += $(wildcard src/*.c)
CSRCS += ../packets/packet.c

include $(APPDIR)/Application.mk
