# Project name
TARGET := menu

SRC_DIR := src
INC_DIR := include
INTERNAL_DIR := $(SRC_DIR)/internal

BUILD ?= debug

ifeq ($(BUILD), release)
	BUILD_DIR := build/release
	OPT_FLAGS := -O3
else
	BUILD_DIR := build/debug
	OPT_FLAGS := -g -O0 -DDEBUG_BUILD
endif

# Compiler
CC := gcc

SRCS := $(shell find $(SRC_DIR) -name "*.c")
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

CFLAGS := -Wall -Wextra -std=c11 -I$(INC_DIR) -I$(INTERNAL_DIR) \
					$(shell pkg-config --cflags sdl2 SDL2_ttf SDL2_image) $(OPT_FLAGS)

LDFLAGS := $(shell pkg-config --libs sdl2 SDL2_ttf SDL2_image) -lgpiod -lm -lGLESv2 -lcglm

all: $(TARGET)

# Link
$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

# Compile
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Ensure build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Run
run: $(TARGET)
	./$(TARGET)

# Clean
clean:
	rm -rf build $(TARGET)

.PHONY: all clean run
