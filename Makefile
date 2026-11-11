CC      ?= cc
.DEFAULT_GOAL := all
CFLAGS  ?= -O2 -g
CFLAGS  += -std=c11 -D_GNU_SOURCE -Wall -Wextra -Wno-unused-parameter
LDLIBS  += -lm

CORE := src/core/gyro_aim.c src/core/profile.c src/core/app.c src/core/webui.c src/core/webui_page.c

src/core/webui_page.c: src/core/webui.html src/core/practice.html tools/embed_webui.py
	python3 tools/embed_webui.py

all: build/gyroaim-linux build/test_core

build/gyroaim-linux: $(CORE) src/linux/main.c src/linux/vpad.c src/linux/vpad.h src/core/*.h | build
	$(CC) $(CFLAGS) -o $@ $(CORE) src/linux/main.c src/linux/vpad.c $(LDLIBS)

build/test_core: $(CORE) tests/test_core.c src/core/*.h | build
	$(CC) $(CFLAGS) -o $@ $(CORE) tests/test_core.c $(LDLIBS)

build:
	mkdir -p build

build/test_vpad: src/linux/vpad.c src/linux/vpad.h tests/test_vpad.c | build
	$(CC) $(CFLAGS) -o $@ src/linux/vpad.c tests/test_vpad.c

test: build/test_core build/test_vpad
	./build/test_core
	./build/test_vpad
	python3 tests/test_remote_play.py

build/test_uinput: src/core/gyro_aim.c src/core/gyro_aim.h src/linux/vpad.c src/linux/vpad.h tests/test_uinput.c | build
	$(CC) $(CFLAGS) -o $@ src/core/gyro_aim.c src/linux/vpad.c tests/test_uinput.c $(LDLIBS)

test-linux: build/test_uinput
	./build/test_uinput

test-http: build/gyroaim-linux
	python3 tests/test_http.py

ps5: src/core/webui_page.c
	$(MAKE) -f src/ps5/Makefile

run: build/gyroaim-linux
	./build/gyroaim-linux

clean:
	rm -rf build

.PHONY: all test test-http test-linux ps5 run clean
