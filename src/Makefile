CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra
PREFIX  ?= /usr/local

SDL_CFLAGS := $(shell pkg-config --cflags sdl3)
SDL_LIBS   := $(shell pkg-config --libs sdl3)

TARGET := qoiviewer
SRCS   := main.c qoi.c img_resize.c img_sharpen.c
OBJS   := $(SRCS:.c=.o)

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(SDL_LIBS) -lm

%.o: %.c
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -MMD -MP -c -o $@ $<

install: $(TARGET)
	install -D -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)

clean:
	rm -f $(OBJS) $(OBJS:.o=.d) $(TARGET)

-include $(OBJS:.o=.d)
