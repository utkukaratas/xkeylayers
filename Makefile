INSTALL=install
PREFIX=/usr

TARGET := xkeylayers

CC = g++
CFLAGS += -Wall -g
CFLAGS += `pkg-config --cflags xtst x11` -std=c++11
LDFLAGS += `pkg-config --libs xtst x11` -pthread


$(TARGET): xkeylayers.cpp
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

install:
	$(INSTALL) -Dm 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)

clean:
	rm $(TARGET)

.PHONY: clean
