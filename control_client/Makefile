###################
### NUTTX BUILD ###
###################
ifneq ($(APPDIR),)

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

#############################
### REGULAR DESKTOP BUILD ###
#############################
else

CC = gcc
CFLAGS = -Wall -Wextra -DDESKTOP_BUILD
OUT = control

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
