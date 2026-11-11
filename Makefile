CC      ?= cc
CFLAGS  ?= -O2 -g
CFLAGS  += -std=c11 -D_GNU_SOURCE -Wall -Wextra -Wno-unused-parameter
LDLIBS  += -lm

CORE := src/core/gyro_aim.c src/core/profile.c src/core/app.c src/core/webui.c src/core/webui_page.c

all: build/gyroaim-linux build/test_core

build/gyroaim-linux: $(CORE) src/linux/main.c src/linux/vpad.c src/linux/vpad.h src/core/*.h | build
	$(CC) $(CFLAGS) -o $@ $(CORE) src/linux/main.c src/linux/vpad.c $(LDLIBS)

build/test_core: $(CORE) tests/test_core.c src/core/*.h | build
	$(CC) $(CFLAGS) -o $@ $(CORE) tests/test_core.c $(LDLIBS)

build:
	mkdir -p build

test: build/test_core
	./build/test_core

run: build/gyroaim-linux
	./build/gyroaim-linux

clean:
	rm -rf build

.PHONY: all test run clean
