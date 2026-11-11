CC      ?= cc
.DEFAULT_GOAL := all
CFLAGS  ?= -O2 -g
CFLAGS  += -std=c11 -D_GNU_SOURCE -Wall -Wextra -Wno-unused-parameter
LDLIBS  += -lm

CORE := src/core/native_protocol.c src/core/gyro_aim.c src/core/profile.c src/core/app.c src/core/webui.c src/core/webui_page.c

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

build/test_native_client: tests/test_native_client.c src/native/client.c src/native/client.h src/core/native_protocol.c src/core/native_protocol.h src/core/profile.c src/core/gyro_aim.c | build
	$(CC) $(CFLAGS) -o $@ tests/test_native_client.c src/native/client.c src/core/native_protocol.c src/core/profile.c src/core/gyro_aim.c $(LDLIBS)

test-native: build/gyroaim-linux build/test_native_client
	python3 tests/test_native.py

build/test_uinput: src/core/gyro_aim.c src/core/gyro_aim.h src/linux/vpad.c src/linux/vpad.h tests/test_uinput.c | build
	$(CC) $(CFLAGS) -o $@ src/core/gyro_aim.c src/linux/vpad.c tests/test_uinput.c $(LDLIBS)

test-linux: build/test_uinput
	./build/test_uinput

test-http: build/gyroaim-linux
	python3 tests/test_http.py

ps5-deps:
	python3 tools/fetch_ps5_native.py

native-lab:
	python3 tools/build_ps5.py --lab

ps5-service: src/core/webui_page.c
	python3 tools/build_ps5.py --service-only

ps5: src/core/webui_page.c
	python3 tools/build_ps5.py
	python3 tests/test_ps5_bundle.py

run: build/gyroaim-linux
	./build/gyroaim-linux

clean:
	rm -rf build

.PHONY: all test test-http test-linux test-native ps5 ps5-service ps5-deps native-lab run clean
