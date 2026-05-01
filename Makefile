.RECIPEPREFIX := >
MAKEFLAGS += --no-print-directory

HOST_CC ?= gcc
CC ?= gcc
KERNEL_CXX ?= g++
LD ?= ld
RUSTC ?= rustc
NM ?= nm
TAR ?= tar
XORRISO ?= xorriso
QEMU_X86_64 ?= qemu-system-x86_64
OPT_LEVEL ?=
MENUCONFIG_ARGS ?=
MENUCONFIG_PRESET ?=
DISK_IMAGE_MB ?= 64
BDT_BUILD_DIR ?= build/bdt
BDT_NAME := bdt
JOBS ?= 1
V ?= 0
SHOW_COMMANDS ?= 0

ifeq ($(OS),Windows_NT)
BDT_NAME := bdt.exe
endif
BDT ?= $(BDT_BUILD_DIR)/$(BDT_NAME)
BDT_SRC := $(wildcard bdt/src/*.c)
BDT_LDLIBS :=
ifneq ($(OS),Windows_NT)
BDT_LDLIBS += -ldl
endif

BDT_VERBOSE :=
ifneq ($(filter 1 ON on TRUE true YES yes Y y,$(SHOW_COMMANDS) $(V)),)
BDT_VERBOSE := --verbose
endif

BDT_CONFIG_VARS := CC="$(CC)" KERNEL_CXX="$(KERNEL_CXX)" LD="$(LD)" RUSTC="$(RUSTC)" NM="$(NM)" TAR="$(TAR)" XORRISO="$(XORRISO)" QEMU_X86_64="$(QEMU_X86_64)" opt_level="$(OPT_LEVEL)" menuconfig_args="$(MENUCONFIG_ARGS)" menuconfig_preset="$(if $(MENUCONFIG_PRESET),--preset $(MENUCONFIG_PRESET),)" DISK_IMAGE_MB="$(DISK_IMAGE_MB)"

.PHONY: all bdt configure reconfigure menuconfig menuconfig-gui menuconfig-clks menuconfig-gui-clks setup setup-tools setup-limine kernel kernel-symbols userapps ramdisk-root ramdisk disk-image iso run debug clean-drive-image clean clean-all help list scan graph

all: iso

bdt: $(BDT)

$(BDT): $(BDT_SRC) bdt/src/bdt.h
> @mkdir -p $(BDT_BUILD_DIR)
> $(HOST_CC) -std=c11 -O2 -Wall -Wextra -Ibdt/src $(BDT_SRC) -o $(BDT) $(BDT_LDLIBS)

configure reconfigure setup setup-tools setup-limine kernel kernel-symbols userapps ramdisk-root ramdisk disk-image iso run debug clean-drive-image clean clean-all menuconfig menuconfig-gui menuconfig-clks menuconfig-gui-clks: bdt
> $(BDT_CONFIG_VARS) $(BDT) $@ -j $(JOBS) $(BDT_VERBOSE)

list: bdt
> $(BDT) --list $(BDT_VERBOSE)

scan: bdt
> $(BDT) --scan $(BDT_VERBOSE)

graph: bdt
> $(BDT) --graph

help: bdt
> $(BDT) --list
> @echo ""
> @echo "bdt entrypoints:"
> @echo "  make iso"
> @echo "  make run"
> @echo "  make menuconfig"
> @echo "  make scan"
> @echo "  make graph"
