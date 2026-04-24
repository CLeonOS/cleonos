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
QEMU_DRIVE_IMAGE ?= build/x86_64/cleonos_disk.img
SHOW_COMMANDS ?= 0
V ?= 0

ifneq ($(filter 1 ON on TRUE true YES yes Y y,$(SHOW_COMMANDS) $(V)),)
Q :=
CMAKE_BUILD_VERBOSE_ARG := --verbose
CMAKE_CONFIG_VERBOSE_ARG := -DCMAKE_VERBOSE_MAKEFILE=ON
else
Q := @
CMAKE_BUILD_VERBOSE_ARG :=
CMAKE_CONFIG_VERBOSE_ARG :=
endif

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

.PHONY: all configure reconfigure menuconfig menuconfig-gui menuconfig-clks menuconfig-gui-clks setup setup-tools setup-limine kernel userapps ramdisk-root ramdisk disk-image iso run debug clean-drive-image clean clean-all help

ifeq ($(CLEONOS_ENABLED_BOOL),1)
all: iso
else
all: kernel
endif

configure:
> $(Q)$(CMAKE) -S . -B $(CMAKE_BUILD_DIR) $(GEN_ARG) -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) -DNO_COLOR=$(NO_COLOR) $(CMAKE_CONFIG_VERBOSE_ARG) $(CMAKE_EXTRA_ARGS) $(CMAKE_PASSTHROUGH_ARGS)

reconfigure:
> $(Q)rm -rf $(CMAKE_BUILD_DIR)
> $(Q)$(MAKE) configure CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) CMAKE_GENERATOR="$(CMAKE_GENERATOR)" CMAKE_EXTRA_ARGS="$(CMAKE_EXTRA_ARGS)" NO_COLOR="$(NO_COLOR)" LIMINE_SKIP_CONFIGURE="$(LIMINE_SKIP_CONFIGURE)" LIMINE_REF="$(LIMINE_REF)" LIMINE_REPO="$(LIMINE_REPO)" LIMINE_DIR="$(LIMINE_DIR)" LIMINE_BIN_DIR="$(LIMINE_BIN_DIR)" OBJCOPY_FOR_TARGET="$(OBJCOPY_FOR_TARGET)" OBJDUMP_FOR_TARGET="$(OBJDUMP_FOR_TARGET)" READELF_FOR_TARGET="$(READELF_FOR_TARGET)" CLEONOS_ENABLE="$(CLEONOS_ENABLE)"

menuconfig:
> $(Q)if command -v $(PYTHON) >/dev/null 2>&1; then \
>     $(PYTHON) scripts/menuconfig.py $(MENUCONFIG_SCOPE_ARG) $(MENUCONFIG_PRESET_ARG) $(MENUCONFIG_ARGS); \
> elif command -v python >/dev/null 2>&1; then \
>     python scripts/menuconfig.py $(MENUCONFIG_SCOPE_ARG) $(MENUCONFIG_PRESET_ARG) $(MENUCONFIG_ARGS); \
> else \
>     echo "python3/python not found"; \
>     exit 1; \
> fi
> $(Q)$(MAKE) configure CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) CMAKE_GENERATOR="$(CMAKE_GENERATOR)" CMAKE_EXTRA_ARGS="$(CMAKE_EXTRA_ARGS)" NO_COLOR="$(NO_COLOR)" LIMINE_SKIP_CONFIGURE="$(LIMINE_SKIP_CONFIGURE)" LIMINE_REF="$(LIMINE_REF)" LIMINE_REPO="$(LIMINE_REPO)" LIMINE_DIR="$(LIMINE_DIR)" LIMINE_BIN_DIR="$(LIMINE_BIN_DIR)" OBJCOPY_FOR_TARGET="$(OBJCOPY_FOR_TARGET)" OBJDUMP_FOR_TARGET="$(OBJDUMP_FOR_TARGET)" READELF_FOR_TARGET="$(READELF_FOR_TARGET)" CLEONOS_ENABLE="$(CLEONOS_ENABLE)"

menuconfig-gui:
> $(Q)if command -v $(PYTHON) >/dev/null 2>&1; then \
>     $(PYTHON) scripts/menuconfig.py --gui $(MENUCONFIG_SCOPE_ARG) $(MENUCONFIG_PRESET_ARG) $(MENUCONFIG_ARGS); \
> elif command -v python >/dev/null 2>&1; then \
>     python scripts/menuconfig.py --gui $(MENUCONFIG_SCOPE_ARG) $(MENUCONFIG_PRESET_ARG) $(MENUCONFIG_ARGS); \
> else \
>     echo "python3/python not found"; \
>     exit 1; \
> fi
> $(Q)$(MAKE) configure CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) CMAKE_GENERATOR="$(CMAKE_GENERATOR)" CMAKE_EXTRA_ARGS="$(CMAKE_EXTRA_ARGS)" NO_COLOR="$(NO_COLOR)" LIMINE_SKIP_CONFIGURE="$(LIMINE_SKIP_CONFIGURE)" LIMINE_REF="$(LIMINE_REF)" LIMINE_REPO="$(LIMINE_REPO)" LIMINE_DIR="$(LIMINE_DIR)" LIMINE_BIN_DIR="$(LIMINE_BIN_DIR)" OBJCOPY_FOR_TARGET="$(OBJCOPY_FOR_TARGET)" OBJDUMP_FOR_TARGET="$(OBJDUMP_FOR_TARGET)" READELF_FOR_TARGET="$(READELF_FOR_TARGET)" CLEONOS_ENABLE="$(CLEONOS_ENABLE)"

menuconfig-clks:
> $(Q)if command -v $(PYTHON) >/dev/null 2>&1; then \
>     $(PYTHON) scripts/menuconfig.py --clks-only $(MENUCONFIG_PRESET_ARG) $(MENUCONFIG_ARGS); \
> elif command -v python >/dev/null 2>&1; then \
>     python scripts/menuconfig.py --clks-only $(MENUCONFIG_PRESET_ARG) $(MENUCONFIG_ARGS); \
> else \
>     echo "python3/python not found"; \
>     exit 1; \
> fi
> $(Q)$(MAKE) configure CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) CMAKE_GENERATOR="$(CMAKE_GENERATOR)" CMAKE_EXTRA_ARGS="$(CMAKE_EXTRA_ARGS)" NO_COLOR="$(NO_COLOR)" LIMINE_SKIP_CONFIGURE="$(LIMINE_SKIP_CONFIGURE)" LIMINE_REF="$(LIMINE_REF)" LIMINE_REPO="$(LIMINE_REPO)" LIMINE_DIR="$(LIMINE_DIR)" LIMINE_BIN_DIR="$(LIMINE_BIN_DIR)" OBJCOPY_FOR_TARGET="$(OBJCOPY_FOR_TARGET)" OBJDUMP_FOR_TARGET="$(OBJDUMP_FOR_TARGET)" READELF_FOR_TARGET="$(READELF_FOR_TARGET)" CLEONOS_ENABLE="$(CLEONOS_ENABLE)"

menuconfig-gui-clks:
> $(Q)if command -v $(PYTHON) >/dev/null 2>&1; then \
>     $(PYTHON) scripts/menuconfig.py --gui --clks-only $(MENUCONFIG_PRESET_ARG) $(MENUCONFIG_ARGS); \
> elif command -v python >/dev/null 2>&1; then \
>     python scripts/menuconfig.py --gui --clks-only $(MENUCONFIG_PRESET_ARG) $(MENUCONFIG_ARGS); \
> else \
>     echo "python3/python not found"; \
>     exit 1; \
> fi
> $(Q)$(MAKE) configure CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) CMAKE_GENERATOR="$(CMAKE_GENERATOR)" CMAKE_EXTRA_ARGS="$(CMAKE_EXTRA_ARGS)" NO_COLOR="$(NO_COLOR)" LIMINE_SKIP_CONFIGURE="$(LIMINE_SKIP_CONFIGURE)" LIMINE_REF="$(LIMINE_REF)" LIMINE_REPO="$(LIMINE_REPO)" LIMINE_DIR="$(LIMINE_DIR)" LIMINE_BIN_DIR="$(LIMINE_BIN_DIR)" OBJCOPY_FOR_TARGET="$(OBJCOPY_FOR_TARGET)" OBJDUMP_FOR_TARGET="$(OBJDUMP_FOR_TARGET)" READELF_FOR_TARGET="$(READELF_FOR_TARGET)" CLEONOS_ENABLE="$(CLEONOS_ENABLE)"

setup: configure
> $(Q)$(CMAKE) --build $(CMAKE_BUILD_DIR) --target setup $(CMAKE_BUILD_VERBOSE_ARG)

setup-tools: configure
> $(Q)$(CMAKE) --build $(CMAKE_BUILD_DIR) --target setup-tools $(CMAKE_BUILD_VERBOSE_ARG)

setup-limine: configure
> $(Q)$(CMAKE) --build $(CMAKE_BUILD_DIR) --target setup-limine $(CMAKE_BUILD_VERBOSE_ARG)

kernel: configure
> $(Q)$(CMAKE) --build $(CMAKE_BUILD_DIR) --target kernel $(CMAKE_BUILD_VERBOSE_ARG)

ifeq ($(CLEONOS_ENABLED_BOOL),1)
userapps: configure
> $(Q)$(CMAKE) --build $(CMAKE_BUILD_DIR) --target userapps $(CMAKE_BUILD_VERBOSE_ARG)

ramdisk-root: configure
> $(Q)$(CMAKE) --build $(CMAKE_BUILD_DIR) --target ramdisk-root $(CMAKE_BUILD_VERBOSE_ARG)

ramdisk: configure
> $(Q)$(CMAKE) --build $(CMAKE_BUILD_DIR) --target ramdisk $(CMAKE_BUILD_VERBOSE_ARG)

disk-image: configure
> $(Q)$(CMAKE) --build $(CMAKE_BUILD_DIR) --target disk-image $(CMAKE_BUILD_VERBOSE_ARG)

iso: configure
> $(Q)$(CMAKE) --build $(CMAKE_BUILD_DIR) --target iso $(CMAKE_BUILD_VERBOSE_ARG)

run: configure
> $(Q)$(CMAKE) --build $(CMAKE_BUILD_DIR) --target run $(CMAKE_BUILD_VERBOSE_ARG)

debug: configure
> $(Q)$(CMAKE) --build $(CMAKE_BUILD_DIR) --target debug $(CMAKE_BUILD_VERBOSE_ARG)
else
userapps ramdisk-root ramdisk disk-image iso run debug:
> $(Q)echo "target '$@' requires CLEONOS_ENABLE=ON and cleonos sources present"
> $(Q)exit 1
endif

clean:
> $(Q)if [ -d "$(CMAKE_BUILD_DIR)" ]; then \
>     $(CMAKE) --build $(CMAKE_BUILD_DIR) --target clean-x86 $(CMAKE_BUILD_VERBOSE_ARG); \
> else \
>     rm -rf build/x86_64; \
> fi
> $(Q)$(MAKE) clean-drive-image QEMU_DRIVE_IMAGE="$(QEMU_DRIVE_IMAGE)"

clean-all:
> $(Q)if [ -d "$(CMAKE_BUILD_DIR)" ]; then \
>     $(CMAKE) --build $(CMAKE_BUILD_DIR) --target clean-all $(CMAKE_BUILD_VERBOSE_ARG); \
> else \
>     rm -rf build build-cmake; \
> fi
> $(Q)$(MAKE) clean-drive-image QEMU_DRIVE_IMAGE="$(QEMU_DRIVE_IMAGE)"

clean-drive-image:
> $(Q)rm -f "$(QEMU_DRIVE_IMAGE)"

help:
> $(Q)echo "CLeonOS (CMake-backed wrapper)"
> $(Q)echo "Mode: CLEONOS_ENABLE=$(CLEONOS_ENABLE_EFFECTIVE) ($(CLEONOS_MODE_LABEL))"
> $(Q)echo "  make configure"
> $(Q)echo "  make menuconfig"
> $(Q)echo "  make menuconfig-gui"
> $(Q)echo "  make menuconfig-clks"
> $(Q)echo "  make menuconfig-gui-clks"
> $(Q)echo "  make setup"
> $(Q)echo "  make userapps"
> $(Q)echo "  make disk-image"
> $(Q)echo "  make iso"
> $(Q)echo "  make run"
> $(Q)echo "  make debug"
> $(Q)echo "  make clean-drive-image"
> $(Q)echo "  make clean"
> $(Q)echo "  make clean-all"
> $(Q)echo ""
> $(Q)echo "Show commands:"
> $(Q)echo "  make run V=1"
> $(Q)echo "  make run SHOW_COMMANDS=1"
> $(Q)echo "  (includes CMake build internals: compiler/linker command lines)"
> $(Q)echo ""
> $(Q)echo "Pass custom CMake cache args via:"
> $(Q)echo "  make configure CMAKE_EXTRA_ARGS='-DLIMINE_SKIP_CONFIGURE=1 -DOBJCOPY_FOR_TARGET=objcopy'"
> $(Q)echo "Direct passthrough is also supported:"
> $(Q)echo "  make run LIMINE_SKIP_CONFIGURE=1"
> $(Q)echo "Kernel-only mode:"
> $(Q)echo "  make kernel CLEONOS_ENABLE=OFF"
> $(Q)echo "  make -C clks kernel"
> $(Q)echo "Disk image size example:"
> $(Q)echo "  make run DISK_IMAGE_MB=128"
> $(Q)echo "Preset examples:"
> $(Q)echo "  make menuconfig MENUCONFIG_PRESET=full"
> $(Q)echo "  make menuconfig MENUCONFIG_PRESET=minimal"
> $(Q)echo "  make menuconfig-gui MENUCONFIG_PRESET=dev"
