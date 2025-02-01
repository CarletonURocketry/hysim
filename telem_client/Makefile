###################
### NUTTX BUILD ###
###################
ifneq ($(APPDIR),)

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

#############################
### REGULAR DESKTOP BUILD ###
#############################
else

CC = gcc
CFLAGS = -Wall -Wextra
OUT = telem_client

SRCDIR = $(abspath ./src)
SRCS = $(wildcard $(SRCDIR)/*.c)
SRCS += $(wildcard ../packets/*.c)

OBJS = $(patsubst %.c,%.o,$(SRCS))

all: $(OUT)

$(OUT): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(OUT)

%.o: %.c
	$(CC) $(CFLAGS) $(WARNINGS) -o $@ -c $<

clean:
	@rm $(OUT)
	@rm $(OBJS)

endif
