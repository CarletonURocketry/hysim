###################
### NUTTX BUILD ###
###################
ifneq ($(APPDIR),)

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
CSRCS := $(filter-out src/gpio_dummy_actuator.c, $(CSRCS))

include $(APPDIR)/Application.mk

#############################
### REGULAR DESKTOP BUILD ###
#############################
else

CC = gcc
CFLAGS = -Wall -Wextra -pthread -DDESKTOP_BUILD
OUT = pad

SRCDIR = $(abspath ./src)
SRCS = $(wildcard $(SRCDIR)/*.c)
SRCS += $(wildcard ../packets/*.c)
EXCLUDE_SRCS = $(SRCDIR)/gpio_actuator.c 
SRCS := $(filter-out $(EXCLUDE_SRCS), $(SRCS))

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
