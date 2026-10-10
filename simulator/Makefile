CXX ?= clang++
CC ?= clang

CXXFLAGS := -std=c++17 -O2 -Wall -Wno-vla-cxx-extension \
	-I../include \
	-I. \
	-I../.pio/libdeps/seeed_reterminal_sticky/QRCode/src \
	-I../.pio/libdeps/seeed_reterminal_sticky/ArduinoJson/src \
	$(shell pkg-config --cflags sdl2 2>/dev/null || echo "-I/opt/homebrew/include -I/opt/homebrew/include/SDL2")

CFLAGS := -O2 -Wall \
	-I../.pio/libdeps/seeed_reterminal_sticky/QRCode/src

LDFLAGS := $(shell pkg-config --libs sdl2 2>/dev/null || echo "-L/opt/homebrew/lib -lSDL2") \
	-lcurl

BUILD_DIR := build
TARGET := ../screentinker_sim

OBJS := \
	$(BUILD_DIR)/main_sim.o \
	$(BUILD_DIR)/sim_config.o \
	$(BUILD_DIR)/sim_display.o \
	$(BUILD_DIR)/sim_http.o \
	$(BUILD_DIR)/setup_screen.o \
	$(BUILD_DIR)/qrcode.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@
	@echo "=========================================="
	@echo " Successfully built: $@"
	@echo "=========================================="

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/setup_screen.o: ../src/setup_screen.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/qrcode.o: ../.pio/libdeps/seeed_reterminal_sticky/QRCode/src/qrcode.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

.PHONY: all clean
