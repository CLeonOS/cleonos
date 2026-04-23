.RECIPEPREFIX := >
MAKEFLAGS += --no-print-directory

CMAKE ?= cmake
CMAKE_BUILD_DIR ?= build-cmake
CMAKE_BUILD_TYPE ?= Release
CMAKE_GENERATOR ?=
CMAKE_EXTRA_ARGS ?=
APPS ?=
WERROR ?= ON
KIT_CC ?=
KIT_LD ?=

ifeq ($(strip $(CMAKE_GENERATOR)),)
GEN_ARG :=
else
GEN_ARG := -G "$(CMAKE_GENERATOR)"
endif

APPS_ARG :=
ifneq ($(strip $(APPS)),)
APPS_ARG := -DCLEONOS_KIT_APP_NAMES=$(APPS)
endif

KIT_CC_ARG :=
ifneq ($(strip $(KIT_CC)),)
KIT_CC_ARG := -DCLEONOS_KIT_CC=$(KIT_CC)
endif

KIT_LD_ARG :=
ifneq ($(strip $(KIT_LD)),)
KIT_LD_ARG := -DCLEONOS_KIT_LD=$(KIT_LD)
endif

.PHONY: all configure reconfigure apps clean clean-all help

all: apps

configure:
> @$(CMAKE) -S . -B $(CMAKE_BUILD_DIR) $(GEN_ARG) -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) -DCLEONOS_KIT_ENABLE_WERROR=$(WERROR) $(APPS_ARG) $(KIT_CC_ARG) $(KIT_LD_ARG) $(CMAKE_EXTRA_ARGS)

reconfigure:
> @$(CMAKE) -E rm -rf $(CMAKE_BUILD_DIR)
> @$(MAKE) configure CMAKE_BUILD_TYPE="$(CMAKE_BUILD_TYPE)" CMAKE_GENERATOR="$(CMAKE_GENERATOR)" CMAKE_EXTRA_ARGS="$(CMAKE_EXTRA_ARGS)" APPS="$(APPS)" WERROR="$(WERROR)" KIT_CC="$(KIT_CC)" KIT_LD="$(KIT_LD)"

apps: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target apps

clean:
> @if [ -d "$(CMAKE_BUILD_DIR)" ]; then \
>     $(CMAKE) --build $(CMAKE_BUILD_DIR) --target clean; \
> fi

clean-all:
> @$(CMAKE) -E rm -rf $(CMAKE_BUILD_DIR) build

help:
> @echo "CLeonOS kit build wrapper"
> @echo "  make apps"
> @echo "  make apps APPS=hello"
> @echo "  make apps APPS=hello,foo"
> @echo "  make apps KIT_CC=x86_64-elf-gcc KIT_LD=x86_64-elf-ld"
> @echo "  make clean"
> @echo "  make clean-all"
