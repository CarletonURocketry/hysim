CC = gcc
CFLAGS = -Wall -Wextra -pthread -DDESKTOP_BUILD
OUT = pad

SRCDIR = $(abspath ./src)
SRCS = $(wildcard $(SRCDIR)/*.c)
SRCS += $(wildcard ../packets/*.c)

EXCLUDE_SRCS = $(SRCDIR)/gpio_actuator.c
EXCLUDE_SRCS += $(SRCDIR)/pwm_actuator.c

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


