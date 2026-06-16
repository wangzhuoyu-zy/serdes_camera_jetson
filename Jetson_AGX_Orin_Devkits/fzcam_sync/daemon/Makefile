# fzcam_sync_daemon Makefile
#
# Cross-compile for aarch64 (Jetson AGX Orin)

SRC_DIR := $(shell pwd)
BUILD_DIR := $(SRC_DIR)/build
INCLUDE_DIR := $(SRC_DIR)/../include

CROSS_COMPILE ?= aarch64-buildroot-linux-gnu-
CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Wextra -O2 -std=gnu99 -I$(INCLUDE_DIR)
TARGET := fzcam_sync_daemon

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/$(TARGET): $(SRC_DIR)/$(TARGET).c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
