.RECIPEPREFIX := >
MAKEFLAGS += --no-print-directory

ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)

.PHONY: all configure reconfigure kernel menuconfig menuconfig-gui setup setup-tools setup-limine clean clean-all help

all: kernel

configure:
> @$(MAKE) -C "$(ROOT_DIR)" CLEONOS_ENABLE=OFF configure

reconfigure:
> @$(MAKE) -C "$(ROOT_DIR)" CLEONOS_ENABLE=OFF reconfigure

kernel:
> @$(MAKE) -C "$(ROOT_DIR)" CLEONOS_ENABLE=OFF kernel

menuconfig:
> @$(MAKE) -C "$(ROOT_DIR)" CLEONOS_ENABLE=OFF menuconfig-clks

menuconfig-gui:
> @$(MAKE) -C "$(ROOT_DIR)" CLEONOS_ENABLE=OFF menuconfig-gui-clks

setup:
> @$(MAKE) -C "$(ROOT_DIR)" CLEONOS_ENABLE=OFF setup

setup-tools:
> @$(MAKE) -C "$(ROOT_DIR)" CLEONOS_ENABLE=OFF setup-tools

setup-limine:
> @$(MAKE) -C "$(ROOT_DIR)" CLEONOS_ENABLE=OFF setup-limine

clean:
> @$(MAKE) -C "$(ROOT_DIR)" CLEONOS_ENABLE=OFF clean

clean-all:
> @$(MAKE) -C "$(ROOT_DIR)" CLEONOS_ENABLE=OFF clean-all

help:
> @echo "CLKS standalone wrapper"
> @echo "  make -C clks kernel"
> @echo "  make -C clks menuconfig"
> @echo "  make -C clks menuconfig-gui"
> @echo "  make -C clks configure"
> @echo "  make -C clks clean"
> @echo "  make -C clks clean-all"
