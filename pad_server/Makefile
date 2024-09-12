CC = gcc
CFLAGS = -Wall -Wextra
OUT = pad

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
