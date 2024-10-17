include $(APPDIR)/Make.defs

# Application information for control client

PROGNAME = $(CONFIG_HYSIM_CONTROL_CLIENT_PROGNAME)
PRIORITY = $(CONFIG_HYSIM_CONTROL_CLIENT_PRIORITY)
STACKSIZE = $(CONFIG_HYSIM_CONTROL_CLIENT_STACKSIZE)
MODULE = $(CONFIG_HYSIM_CONTROL_CLIENT)

# Control client recipe

MAINSRC = src/control_client_main.c

CSRCS += $(wildcard src/*.c)
CSRCS += ../packets/packet.c

include $(APPDIR)/Application.mk
