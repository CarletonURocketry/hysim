include $(APPDIR)/Make.defs

# Application information for telemetry client

PROGNAME = $(CONFIG_HYSIM_TELEM_CLIENT_PROGNAME)
PRIORITY = $(CONFIG_HYSIM_TELEM_CLIENT_PRIORITY)
STACKSIZE = $(CONFIG_HYSIM_TELEM_CLIENT_STACKSIZE)
MODULE = $(CONFIG_HYSIM_TELEM_CLIENT)

# Telemetry client recipe

MAINSRC = src/telem_client_main.c

CSRCS += $(wildcard src/*.c)
CSRCS += ../packets/packet.c

include $(APPDIR)/Application.mk
