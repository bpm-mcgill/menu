# Project name
TARGET := menu

SRC_DIR := src
INC_DIR := include
BUILD_DIR := build

# Compiler
CC := gcc

SRCS := $(shell find $(SRC_DIR) -name "*.c")
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

CFLAGS := -Wall -Wextra -O0 -std=c11 -I$(INC_DIR) \
					$(shell pkg-config --cflags sdl2 SDL2_ttf SDL2_image)

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
	rm -rf $(BUILD_DIR) $(TARGET)

.PHONY: all clean run
