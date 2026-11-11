# Invoke from repository root: make ps5 PS5_PAYLOAD_SDK=/path/to/sdk
ifndef PS5_PAYLOAD_SDK
$(error Defina PS5_PAYLOAD_SDK com o caminho do ps5-payload-sdk)
endif
PS5_CC := $(PS5_PAYLOAD_SDK)/bin/prospero-clang
PS5_CORE := src/core/gyro_aim.c src/core/profile.c src/core/app.c src/core/webui.c src/core/webui_page.c

build/gyroaim-ps5-panel.elf: $(PS5_CORE) src/ps5/main.c src/core/*.h
	mkdir -p build
	$(PS5_CC) -O2 -std=c11 -D_GNU_SOURCE -Wall -Wextra -Wno-unused-parameter -Wno-unreachable-code-generic-assoc -o $@ $(PS5_CORE) src/ps5/main.c -lSceSystemService -lkernel_sys
