# Makefile for isx031_intrinsics
# Target: Jetson Orin (ARM64 Linux, Ubuntu 22.04)
#
# Cross-compile:
#   make CROSS_COMPILE=/path/to/aarch64-linux-gnu-
#
# Native compile (on Jetson Orin directly):
#   make CROSS_COMPILE=

CROSS_COMPILE ?= /opt/gcc-linaro-11.3_aarch64-linux-gnu/bin/aarch64-buildroot-linux-gnu-
CC      = $(CROSS_COMPILE)gcc
CFLAGS  = -Wall -Wextra -O2 -std=c99
LDFLAGS =
TARGET  = isx031_intrinsics_tool
SRCS    = isx031_flash.c isx031_intrinsics_tool.c
HDRS    = isx031_flash.h

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SRCS)

clean:
	rm -f $(TARGET)