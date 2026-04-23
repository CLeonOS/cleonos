.RECIPEPREFIX := >
MAKEFLAGS += --no-print-directory

CMAKE ?= cmake
CMAKE_BUILD_DIR ?= build-cmake
CMAKE_BUILD_TYPE ?= Release
CMAKE_GENERATOR ?=
CMAKE_EXTRA_ARGS ?=
NO_COLOR ?= 0
LIMINE_SKIP_CONFIGURE ?=
LIMINE_REF ?=
LIMINE_REPO ?=
LIMINE_DIR ?=
LIMINE_BIN_DIR ?=
OBJCOPY_FOR_TARGET ?=
OBJDUMP_FOR_TARGET ?=
READELF_FOR_TARGET ?=
PYTHON ?= python3
MENUCONFIG_ARGS ?=
MENUCONFIG_PRESET ?=
DISK_IMAGE_MB ?=
CLEONOS_ENABLE ?= auto

ifeq ($(strip $(CMAKE_GENERATOR)),)
GEN_ARG :=
else
GEN_ARG := -G "$(CMAKE_GENERATOR)"
endif

CMAKE_PASSTHROUGH_ARGS :=
MENUCONFIG_PRESET_ARG := $(if $(strip $(MENUCONFIG_PRESET)),--preset $(MENUCONFIG_PRESET),)
CLEONOS_SOURCE_PRESENT := $(if $(wildcard cleonos/CMakeLists.txt),1,0)

ifeq ($(strip $(CLEONOS_ENABLE)),auto)
ifeq ($(CLEONOS_SOURCE_PRESENT),1)
CLEONOS_ENABLE_EFFECTIVE := ON
else
CLEONOS_ENABLE_EFFECTIVE := OFF
endif
else
CLEONOS_ENABLE_EFFECTIVE := $(CLEONOS_ENABLE)
endif

ifneq ($(filter 1 ON on TRUE true YES yes Y y,$(CLEONOS_ENABLE_EFFECTIVE)),)
CLEONOS_ENABLED_BOOL := 1
CLEONOS_MODE_LABEL := full
else
CLEONOS_ENABLED_BOOL := 0
CLEONOS_MODE_LABEL := clks-only
endif

MENUCONFIG_SCOPE_ARG :=
ifeq ($(CLEONOS_ENABLED_BOOL),0)
MENUCONFIG_SCOPE_ARG += --clks-only
endif

CMAKE_PASSTHROUGH_ARGS += -DCLEONOS_ENABLE=$(CLEONOS_ENABLE_EFFECTIVE)

ifneq ($(strip $(LIMINE_SKIP_CONFIGURE)),)
CMAKE_PASSTHROUGH_ARGS += -DLIMINE_SKIP_CONFIGURE=$(LIMINE_SKIP_CONFIGURE)
endif
ifneq ($(strip $(LIMINE_REF)),)
CMAKE_PASSTHROUGH_ARGS += -DLIMINE_REF=$(LIMINE_REF)
endif
ifneq ($(strip $(LIMINE_REPO)),)
CMAKE_PASSTHROUGH_ARGS += -DLIMINE_REPO=$(LIMINE_REPO)
endif
ifneq ($(strip $(LIMINE_DIR)),)
CMAKE_PASSTHROUGH_ARGS += -DLIMINE_DIR=$(LIMINE_DIR)
endif
ifneq ($(strip $(LIMINE_BIN_DIR)),)
CMAKE_PASSTHROUGH_ARGS += -DLIMINE_BIN_DIR=$(LIMINE_BIN_DIR)
endif
ifneq ($(strip $(OBJCOPY_FOR_TARGET)),)
CMAKE_PASSTHROUGH_ARGS += -DOBJCOPY_FOR_TARGET=$(OBJCOPY_FOR_TARGET)
endif
ifneq ($(strip $(OBJDUMP_FOR_TARGET)),)
CMAKE_PASSTHROUGH_ARGS += -DOBJDUMP_FOR_TARGET=$(OBJDUMP_FOR_TARGET)
endif
ifneq ($(strip $(READELF_FOR_TARGET)),)
CMAKE_PASSTHROUGH_ARGS += -DREADELF_FOR_TARGET=$(READELF_FOR_TARGET)
endif
ifneq ($(strip $(DISK_IMAGE_MB)),)
CMAKE_PASSTHROUGH_ARGS += -DCLEONOS_DISK_IMAGE_MB=$(DISK_IMAGE_MB)
endif

.PHONY: all configure reconfigure menuconfig menuconfig-gui menuconfig-clks menuconfig-gui-clks setup setup-tools setup-limine kernel userapps ramdisk-root ramdisk disk-image iso run debug clean clean-all help

ifeq ($(CLEONOS_ENABLED_BOOL),1)
all: iso
else
all: kernel
endif

configure:
> @$(CMAKE) -S . -B $(CMAKE_BUILD_DIR) $(GEN_ARG) -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) -DNO_COLOR=$(NO_COLOR) $(CMAKE_EXTRA_ARGS) $(CMAKE_PASSTHROUGH_ARGS)

reconfigure:
> @rm -rf $(CMAKE_BUILD_DIR)
> @$(MAKE) configure CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) CMAKE_GENERATOR="$(CMAKE_GENERATOR)" CMAKE_EXTRA_ARGS="$(CMAKE_EXTRA_ARGS)" NO_COLOR="$(NO_COLOR)" LIMINE_SKIP_CONFIGURE="$(LIMINE_SKIP_CONFIGURE)" LIMINE_REF="$(LIMINE_REF)" LIMINE_REPO="$(LIMINE_REPO)" LIMINE_DIR="$(LIMINE_DIR)" LIMINE_BIN_DIR="$(LIMINE_BIN_DIR)" OBJCOPY_FOR_TARGET="$(OBJCOPY_FOR_TARGET)" OBJDUMP_FOR_TARGET="$(OBJDUMP_FOR_TARGET)" READELF_FOR_TARGET="$(READELF_FOR_TARGET)" CLEONOS_ENABLE="$(CLEONOS_ENABLE)"

menuconfig:
> @if command -v $(PYTHON) >/dev/null 2>&1; then \
>     $(PYTHON) scripts/menuconfig.py $(MENUCONFIG_SCOPE_ARG) $(MENUCONFIG_PRESET_ARG) $(MENUCONFIG_ARGS); \
> elif command -v python >/dev/null 2>&1; then \
>     python scripts/menuconfig.py $(MENUCONFIG_SCOPE_ARG) $(MENUCONFIG_PRESET_ARG) $(MENUCONFIG_ARGS); \
> else \
>     echo "python3/python not found"; \
>     exit 1; \
> fi
> @$(MAKE) configure CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) CMAKE_GENERATOR="$(CMAKE_GENERATOR)" CMAKE_EXTRA_ARGS="$(CMAKE_EXTRA_ARGS)" NO_COLOR="$(NO_COLOR)" LIMINE_SKIP_CONFIGURE="$(LIMINE_SKIP_CONFIGURE)" LIMINE_REF="$(LIMINE_REF)" LIMINE_REPO="$(LIMINE_REPO)" LIMINE_DIR="$(LIMINE_DIR)" LIMINE_BIN_DIR="$(LIMINE_BIN_DIR)" OBJCOPY_FOR_TARGET="$(OBJCOPY_FOR_TARGET)" OBJDUMP_FOR_TARGET="$(OBJDUMP_FOR_TARGET)" READELF_FOR_TARGET="$(READELF_FOR_TARGET)" CLEONOS_ENABLE="$(CLEONOS_ENABLE)"

menuconfig-gui:
> @if command -v $(PYTHON) >/dev/null 2>&1; then \
>     $(PYTHON) scripts/menuconfig.py --gui $(MENUCONFIG_SCOPE_ARG) $(MENUCONFIG_PRESET_ARG) $(MENUCONFIG_ARGS); \
> elif command -v python >/dev/null 2>&1; then \
>     python scripts/menuconfig.py --gui $(MENUCONFIG_SCOPE_ARG) $(MENUCONFIG_PRESET_ARG) $(MENUCONFIG_ARGS); \
> else \
>     echo "python3/python not found"; \
>     exit 1; \
> fi
> @$(MAKE) configure CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) CMAKE_GENERATOR="$(CMAKE_GENERATOR)" CMAKE_EXTRA_ARGS="$(CMAKE_EXTRA_ARGS)" NO_COLOR="$(NO_COLOR)" LIMINE_SKIP_CONFIGURE="$(LIMINE_SKIP_CONFIGURE)" LIMINE_REF="$(LIMINE_REF)" LIMINE_REPO="$(LIMINE_REPO)" LIMINE_DIR="$(LIMINE_DIR)" LIMINE_BIN_DIR="$(LIMINE_BIN_DIR)" OBJCOPY_FOR_TARGET="$(OBJCOPY_FOR_TARGET)" OBJDUMP_FOR_TARGET="$(OBJDUMP_FOR_TARGET)" READELF_FOR_TARGET="$(READELF_FOR_TARGET)" CLEONOS_ENABLE="$(CLEONOS_ENABLE)"

menuconfig-clks:
> @if command -v $(PYTHON) >/dev/null 2>&1; then \
>     $(PYTHON) scripts/menuconfig.py --clks-only $(MENUCONFIG_PRESET_ARG) $(MENUCONFIG_ARGS); \
> elif command -v python >/dev/null 2>&1; then \
>     python scripts/menuconfig.py --clks-only $(MENUCONFIG_PRESET_ARG) $(MENUCONFIG_ARGS); \
> else \
>     echo "python3/python not found"; \
>     exit 1; \
> fi
> @$(MAKE) configure CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) CMAKE_GENERATOR="$(CMAKE_GENERATOR)" CMAKE_EXTRA_ARGS="$(CMAKE_EXTRA_ARGS)" NO_COLOR="$(NO_COLOR)" LIMINE_SKIP_CONFIGURE="$(LIMINE_SKIP_CONFIGURE)" LIMINE_REF="$(LIMINE_REF)" LIMINE_REPO="$(LIMINE_REPO)" LIMINE_DIR="$(LIMINE_DIR)" LIMINE_BIN_DIR="$(LIMINE_BIN_DIR)" OBJCOPY_FOR_TARGET="$(OBJCOPY_FOR_TARGET)" OBJDUMP_FOR_TARGET="$(OBJDUMP_FOR_TARGET)" READELF_FOR_TARGET="$(READELF_FOR_TARGET)" CLEONOS_ENABLE="$(CLEONOS_ENABLE)"

menuconfig-gui-clks:
> @if command -v $(PYTHON) >/dev/null 2>&1; then \
>     $(PYTHON) scripts/menuconfig.py --gui --clks-only $(MENUCONFIG_PRESET_ARG) $(MENUCONFIG_ARGS); \
> elif command -v python >/dev/null 2>&1; then \
>     python scripts/menuconfig.py --gui --clks-only $(MENUCONFIG_PRESET_ARG) $(MENUCONFIG_ARGS); \
> else \
>     echo "python3/python not found"; \
>     exit 1; \
> fi
> @$(MAKE) configure CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) CMAKE_GENERATOR="$(CMAKE_GENERATOR)" CMAKE_EXTRA_ARGS="$(CMAKE_EXTRA_ARGS)" NO_COLOR="$(NO_COLOR)" LIMINE_SKIP_CONFIGURE="$(LIMINE_SKIP_CONFIGURE)" LIMINE_REF="$(LIMINE_REF)" LIMINE_REPO="$(LIMINE_REPO)" LIMINE_DIR="$(LIMINE_DIR)" LIMINE_BIN_DIR="$(LIMINE_BIN_DIR)" OBJCOPY_FOR_TARGET="$(OBJCOPY_FOR_TARGET)" OBJDUMP_FOR_TARGET="$(OBJDUMP_FOR_TARGET)" READELF_FOR_TARGET="$(READELF_FOR_TARGET)" CLEONOS_ENABLE="$(CLEONOS_ENABLE)"

setup: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target setup

setup-tools: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target setup-tools

setup-limine: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target setup-limine

kernel: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target kernel

ifeq ($(CLEONOS_ENABLED_BOOL),1)
userapps: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target userapps

ramdisk-root: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target ramdisk-root

ramdisk: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target ramdisk

disk-image: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target disk-image

iso: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target iso

run: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target run

debug: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target debug
else
userapps ramdisk-root ramdisk disk-image iso run debug:
> @echo "target '$@' requires CLEONOS_ENABLE=ON and cleonos sources present"
> @exit 1
endif

clean:
> @if [ -d "$(CMAKE_BUILD_DIR)" ]; then \
>     $(CMAKE) --build $(CMAKE_BUILD_DIR) --target clean-x86; \
> else \
>     rm -rf build/x86_64; \
> fi

clean-all:
> @if [ -d "$(CMAKE_BUILD_DIR)" ]; then \
>     $(CMAKE) --build $(CMAKE_BUILD_DIR) --target clean-all; \
> else \
>     rm -rf build build-cmake; \
> fi

help:
> @echo "CLeonOS (CMake-backed wrapper)"
> @echo "Mode: CLEONOS_ENABLE=$(CLEONOS_ENABLE_EFFECTIVE) ($(CLEONOS_MODE_LABEL))"
> @echo "  make configure"
> @echo "  make menuconfig"
> @echo "  make menuconfig-gui"
> @echo "  make menuconfig-clks"
> @echo "  make menuconfig-gui-clks"
> @echo "  make setup"
> @echo "  make userapps"
> @echo "  make disk-image"
> @echo "  make iso"
> @echo "  make run"
> @echo "  make debug"
> @echo "  make clean"
> @echo "  make clean-all"
> @echo ""
> @echo "Pass custom CMake cache args via:"
> @echo "  make configure CMAKE_EXTRA_ARGS='-DLIMINE_SKIP_CONFIGURE=1 -DOBJCOPY_FOR_TARGET=objcopy'"
> @echo "Direct passthrough is also supported:"
> @echo "  make run LIMINE_SKIP_CONFIGURE=1"
> @echo "Kernel-only mode:"
> @echo "  make kernel CLEONOS_ENABLE=OFF"
> @echo "  make -C clks kernel"
> @echo "Disk image size example:"
> @echo "  make run DISK_IMAGE_MB=128"
> @echo "Preset examples:"
> @echo "  make menuconfig MENUCONFIG_PRESET=full"
> @echo "  make menuconfig MENUCONFIG_PRESET=minimal"
> @echo "  make menuconfig-gui MENUCONFIG_PRESET=dev"
