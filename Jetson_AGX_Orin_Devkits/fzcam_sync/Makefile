# fzcam_sync kernel module Makefile
#
# Target: NVIDIA Jetson AGX Orin, JetPack 6.2 (aarch64)
#
# Supports both native build (on Jetson) and cross-compile.
#
# Usage:
#   make                  - Build kernel module (auto-detect native/cross)
#   make native           - Force native build on Jetson
#   make daemon           - Build userspace daemon
#   make all              - Build everything
#   make clean            - Clean build artifacts

SRC_DIR := $(shell pwd)

# ---- Auto-detect build type ----
# If kernel is tegra (Jetson), default to native build.
# Set CROSS=1 to override.
NATIVE_BUILD := 0
ifneq ($(CROSS),1)
  KERNEL_NAME := $(shell uname -r 2>/dev/null)
  ifneq (,$(findstring tegra,$(KERNEL_NAME)))
    NATIVE_BUILD := 1
  endif
endif

# ---- Native build (on Jetson) ----
ifeq ($(NATIVE_BUILD),1)
  KDIR          := /lib/modules/$(shell uname -r)/build
  ARCH          := arm64
  CROSS_COMPILE :=
  CC            := gcc
else
  # ---- Cross-compile environment configuration ----
  JP ?= 6.2
  KDIR_BASE     ?= /home/fz/wks/jp6.2.1_r36.4.x/kernel
  TOOLCHAIN_BASE ?= /opt/gcc-linaro-11.3_aarch64-linux-gnu
  KDIR          := $(KDIR_BASE)/kernel-jammy-src
  CROSS_COMPILE := $(TOOLCHAIN_BASE)/bin/aarch64-buildroot-linux-gnu-
  ARCH          := arm64
  CC            := $(CROSS_COMPILE)gcc
endif

# ---- Targets ----
obj-m := fzcam_sync.o

all: build

build:
	@mkdir -p $(SRC_DIR)/build
	@if [ "$(NATIVE_BUILD)" = "1" ]; then \
		echo "[KERNEL] Native build on Jetson ($(shell uname -r))"; \
	else \
		echo "[KERNEL] Cross-compile for JP$(JP)"; \
	fi
	@echo "  KDIR: $(KDIR)"
	$(MAKE) -C $(KDIR) M=$(SRC_DIR) ARCH=$(ARCH) \
		CROSS_COMPILE="$(CROSS_COMPILE)" modules
	@cp $(SRC_DIR)/fzcam_sync.ko $(SRC_DIR)/build/ 2>/dev/null || true
	@echo "[KERNEL] Done: build/fzcam_sync.ko"

native:
	$(MAKE) build CROSS=0

daemon:
	@mkdir -p $(SRC_DIR)/daemon/build
	$(CC) -Wall -Wextra -O2 -std=gnu99 \
		-I$(SRC_DIR)/include \
		-o $(SRC_DIR)/daemon/build/fzcam_sync_daemon \
		$(SRC_DIR)/daemon/fzcam_sync_daemon.c
	@echo "[DAEMON] Done: daemon/build/fzcam_sync_daemon"

clean:
	rm -rf $(SRC_DIR)/build
	rm -rf $(SRC_DIR)/daemon/build
	$(MAKE) -C $(KDIR) M=$(SRC_DIR) ARCH=$(ARCH) \
		CROSS_COMPILE="$(CROSS_COMPILE)" clean 2>/dev/null || true

help:
	@echo "fzcam_sync Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make prepare-kernel - Prepare kernel source tree for external modules"
	@echo "  make build          - Build kernel module (auto native/cross)"
	@echo "  make native         - Force native build on Jetson"
	@echo "  make daemon         - Build userspace daemon"
	@echo "  make all            - Build kernel + daemon"
	@echo "  make clean          - Clean all artifacts"
	@echo ""
	@echo "Cross-compile:"
	@echo "  CROSS=1 make prepare-kernel - Prepare kernel first"
	@echo "  CROSS=1 make build          - Force cross-compile"
	@echo ""
	@echo "Override paths:"
	@echo "  KDIR=/path/to/kernel make build"

.PHONY: all build native daemon clean help prepare-kernel

prepare-kernel:
	@echo "[KERNEL] Preparing kernel source tree at $(KDIR)"
	-$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE="$(CROSS_COMPILE)" modules_prepare 2>/dev/null || true
	@echo "[KERNEL] Generating scripts/module.lds"
	@if [ ! -f $(KDIR)/scripts/module.lds ] && [ -f $(KDIR)/scripts/.module.lds.cmd ]; then \
		CMD=$$(head -1 $(KDIR)/scripts/.module.lds.cmd | sed 's/^cmd_scripts\/module.lds := //'); \
		cd $(KDIR) && eval "$$CMD" 2>/dev/null || true; \
	fi
	@if [ -f $(KDIR)/scripts/module.lds ]; then \
		echo "[KERNEL] module.lds OK: $(KDIR)/scripts/module.lds"; \
	else \
		echo "[KERNEL] Warning: module.lds is missing, please check the kernel source tree"; \
	fi
	@echo "[KERNEL] Kernel prepared successfully"

-include $(SRC_DIR)/env.mk
