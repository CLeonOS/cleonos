cmake_minimum_required(VERSION 3.20)
project(CLeonOS NONE)

# User-configurable toolchain and tool variables (Makefile-compatible defaults).
set(NO_COLOR 0 CACHE STRING "Disable colored log output when set to 1")
include("${CMAKE_SOURCE_DIR}/cmake/log.cmake")
set(CL_LOG_EMIT_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/log_emit.cmake")

set(CC "x86_64-elf-gcc" CACHE STRING "Kernel C compiler")
set(KERNEL_CXX "g++" CACHE STRING "Kernel C++ compiler")
set(LD "x86_64-elf-ld" CACHE STRING "Kernel linker")
set(USER_CC "cc" CACHE STRING "User-space C compiler")
set(USER_LD "ld" CACHE STRING "User-space linker")
set(RUSTC "rustc" CACHE STRING "Rust compiler")
set(NM "nm" CACHE STRING "nm executable")
set(ADDR2LINE "addr2line" CACHE STRING "addr2line executable")

set(XORRISO "xorriso" CACHE STRING "xorriso executable")
set(TAR "tar" CACHE STRING "tar executable")
set(GIT_TOOL "git" CACHE STRING "git executable")
set(MAKE_TOOL "make" CACHE STRING "make executable")
set(SH_TOOL "sh" CACHE STRING "POSIX shell executable")

set(OBJCOPY_FOR_TARGET "llvm-objcopy" CACHE STRING "objcopy tool for limine configure")
set(OBJDUMP_FOR_TARGET "llvm-objdump" CACHE STRING "objdump tool for limine configure")
set(READELF_FOR_TARGET "llvm-readelf" CACHE STRING "readelf tool for limine configure")

set(QEMU_X86_64 "qemu-system-x86_64" CACHE STRING "QEMU executable")

set(CLEONOS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/cleonos")
if(EXISTS "${CLEONOS_SOURCE_DIR}/CMakeLists.txt")
    set(_cleonos_enable_default ON)
else()
    set(_cleonos_enable_default OFF)
endif()
set(CLEONOS_ENABLE ${_cleonos_enable_default} CACHE BOOL "Enable CLeonOS userland/ramdisk/iso integration")
if(CLEONOS_ENABLE AND NOT EXISTS "${CLEONOS_SOURCE_DIR}/CMakeLists.txt")
    cl_log_error("CLEONOS_ENABLE=ON but missing ${CLEONOS_SOURCE_DIR}/CMakeLists.txt")
endif()
if(NOT CLEONOS_ENABLE)
    cl_log_warn("CLEONOS_ENABLE=OFF, running in CLKS-only build mode")
endif()

set(LIMINE_DIR "limine" CACHE STRING "Limine source directory")
set(LIMINE_REPO "https://gh-proxy.com/https://github.com/limine-bootloader/limine.git" CACHE STRING "Limine git repository")
set(LIMINE_REF "" CACHE STRING "Optional Limine branch or tag")
set(LIMINE_BIN_DIR "${LIMINE_DIR}/bin" CACHE STRING "Limine bin directory")
set(LIMINE_CONFIGURE_FLAGS "--enable-bios-cd --enable-uefi-cd --enable-uefi-x86-64" CACHE STRING "Limine configure flags")
set(LIMINE_SKIP_CONFIGURE 0 CACHE BOOL "Skip Limine configure step")

function(resolve_path INPUT_PATH OUTPUT_VAR)
    if(IS_ABSOLUTE "${INPUT_PATH}")
        set(${OUTPUT_VAR} "${INPUT_PATH}" PARENT_SCOPE)
    else()
        set(${OUTPUT_VAR} "${CMAKE_SOURCE_DIR}/${INPUT_PATH}" PARENT_SCOPE)
    endif()
endfunction()

function(resolve_tool_with_fallback VAR_NAME)
    set(_fallbacks ${ARGN})
    set(_requested "${${VAR_NAME}}")

    if("${_requested}" STREQUAL "")
        cl_log_error("empty tool variable: ${VAR_NAME}")
    endif()

    if(IS_ABSOLUTE "${_requested}")
        if(NOT EXISTS "${_requested}")
            cl_log_error("${VAR_NAME} not found: ${_requested}")
        endif()
        set(_resolved "${_requested}")
    else()
        unset(_requested_path CACHE)
        unset(_requested_path)
        find_program(_requested_path NAMES "${_requested}")
        if(_requested_path)
            set(_resolved "${_requested_path}")
        else()
            set(_resolved "")
            foreach(_cand IN LISTS _fallbacks)
                unset(_cand_path CACHE)
                unset(_cand_path)
                find_program(_cand_path NAMES "${_cand}")
                if(_cand_path)
                    cl_log_warn("${VAR_NAME} '${_requested}' not found; fallback to '${_cand}'")
                    set(_resolved "${_cand_path}")
                    break()
                endif()
            endforeach()
            if("${_resolved}" STREQUAL "")
                cl_log_error("${VAR_NAME} tool not found: '${_requested}', fallbacks='${_fallbacks}'")
            endif()
        endif()
    endif()

    get_property(_help CACHE "${VAR_NAME}" PROPERTY HELPSTRING)
    if("${_help}" STREQUAL "")
        set(_help "tool path/name")
    endif()
    set(${VAR_NAME} "${_resolved}" CACHE STRING "${_help}" FORCE)
    set(${VAR_NAME} "${_resolved}" PARENT_SCOPE)
endfunction()

set(BUILD_ROOT "${CMAKE_SOURCE_DIR}/build/x86_64")
set(OBJ_ROOT "${BUILD_ROOT}/obj")
set(ISO_ROOT "${BUILD_ROOT}/iso_root")
set(RAMDISK_ROOT "${BUILD_ROOT}/ramdisk_root")
set(KERNEL_ELF "${BUILD_ROOT}/clks_kernel.elf")
set(RAMDISK_IMAGE "${BUILD_ROOT}/cleonos_ramdisk.tar")
set(DISK_IMAGE "${BUILD_ROOT}/cleonos_disk.img")
set(CLEONOS_DISK_IMAGE_MB "64" CACHE STRING "Default runtime disk image size in MB")
set(ISO_IMAGE "${CMAKE_SOURCE_DIR}/build/CLeonOS-x86_64.iso")
set(KERNEL_SYMBOLS_FILE "${BUILD_ROOT}/kernel.sym")

set(USER_BUILD_ROOT "${BUILD_ROOT}/user")
set(USER_OBJ_ROOT "${USER_BUILD_ROOT}/obj")
set(USER_APP_DIR "${USER_BUILD_ROOT}/apps")
set(USER_LIB_DIR "${USER_BUILD_ROOT}/lib")
set(USER_RUST_LIB "${USER_LIB_DIR}/libcleonos_user_rust.a")
set(KERNEL_RUST_LIB "${BUILD_ROOT}/libclks_kernel_rust.a")

resolve_tool_with_fallback(CC gcc cc clang)
resolve_tool_with_fallback(KERNEL_CXX g++ clang++ x86_64-elf-g++)
resolve_tool_with_fallback(LD ld.lld ld)
if(CLEONOS_ENABLE)
    resolve_tool_with_fallback(USER_CC cc gcc clang)
    resolve_tool_with_fallback(USER_LD ld.lld ld)
endif()
resolve_tool_with_fallback(NM llvm-nm x86_64-elf-nm nm)
resolve_tool_with_fallback(ADDR2LINE llvm-addr2line x86_64-elf-addr2line addr2line)
resolve_tool_with_fallback(OBJCOPY_FOR_TARGET llvm-objcopy x86_64-linux-gnu-objcopy objcopy)
resolve_tool_with_fallback(OBJDUMP_FOR_TARGET llvm-objdump x86_64-linux-gnu-objdump objdump)
resolve_tool_with_fallback(READELF_FOR_TARGET llvm-readelf x86_64-linux-gnu-readelf readelf)

resolve_path("${LIMINE_DIR}" LIMINE_DIR_ABS)
resolve_path("${LIMINE_BIN_DIR}" LIMINE_BIN_DIR_ABS)
set(LIMINE_SETUP_STAMP "${LIMINE_DIR_ABS}/.cleonos-limine-setup.stamp")
set(LIMINE_BUILD_STAMP "${LIMINE_DIR_ABS}/.cleonos-limine-build.stamp")

set(CLKS_ARCH "x86_64" CACHE STRING "Target CLKS arch")
set(USER_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/cleonos/c/user.ld")
set(KELF_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/cleonos/c/kelf.ld")
set(CLEONOS_MENUCONFIG_CMAKE "${CMAKE_SOURCE_DIR}/configs/menuconfig/config.cmake" CACHE FILEPATH "menuconfig generated CMake config")

if(EXISTS "${CLEONOS_MENUCONFIG_CMAKE}")
    include("${CLEONOS_MENUCONFIG_CMAKE}")
    cl_log_info("menuconfig loaded from ${CLEONOS_MENUCONFIG_CMAKE}")
endif()

# CLKS compile flags and feature-to-macro expansion now live in the CLKS submodule build system.
set(USER_CFLAGS
    -std=c11
    -ffreestanding
    -fno-stack-protector
    -fno-builtin
    -Wall
    -Wextra
    -Werror
    "-I${CMAKE_SOURCE_DIR}/cleonos/c/include"
)

set(USER_CFLAGS_DOOM
    -std=c11
    -ffreestanding
    -fno-stack-protector
    -fno-builtin
    -Wall
    -Wextra
    -Wno-error
    -Wno-unused-parameter
    -Wno-missing-field-initializers
    -Wno-sign-compare
    -D_DEFAULT_SOURCE
    -D_POSIX_C_SOURCE=200809L
    "-I${CMAKE_SOURCE_DIR}/cleonos/third-party/doomgeneric/doomgeneric"
    "-include"
    "${CMAKE_SOURCE_DIR}/cleonos/c/apps/doom/doom_shim.h"
)

set(USER_LDFLAGS
    -nostdlib
    -z
    max-page-size=0x1000
    -T
    "${USER_LINKER_SCRIPT}"
)

set(KELF_LDFLAGS
    -nostdlib
    -z
    max-page-size=0x1000
    -T
    "${KELF_LINKER_SCRIPT}"
)

if(NOT CLKS_ARCH STREQUAL "x86_64")
    cl_log_error("CLKS_ARCH=${CLKS_ARCH} is not supported by this CMake build yet; use x86_64")
endif()

set(CLKS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/clks")
set(CLKS_CMAKE_BUILD_DIR "${CMAKE_BINARY_DIR}/clks-cmake")
if(NOT EXISTS "${CLKS_SOURCE_DIR}/CMakeLists.txt")
    cl_log_error("missing CLKS submodule CMakeLists: ${CLKS_SOURCE_DIR}/CMakeLists.txt")
endif()

set(CLKS_SUBMODULE_CMAKE_ARGS
    "-DNO_COLOR=${NO_COLOR}"
    "-DCLKS_ARCH=${CLKS_ARCH}"
    "-DCLKS_BUILD_ROOT=${BUILD_ROOT}"
    "-DCLKS_MENUCONFIG_CMAKE=${CMAKE_SOURCE_DIR}/configs/menuconfig/config.cmake"
    "-DCC=${CC}"
    "-DKERNEL_CXX=${KERNEL_CXX}"
    "-DLD=${LD}"
    "-DRUSTC=${RUSTC}"
    "-DNM=${NM}"
    "-DADDR2LINE=${ADDR2LINE}"
    "-DOBJCOPY_FOR_TARGET=${OBJCOPY_FOR_TARGET}"
    "-DOBJDUMP_FOR_TARGET=${OBJDUMP_FOR_TARGET}"
    "-DREADELF_FOR_TARGET=${READELF_FOR_TARGET}"
)

add_custom_target(kernel
    COMMAND ${CMAKE_COMMAND} -S "${CLKS_SOURCE_DIR}" -B "${CLKS_CMAKE_BUILD_DIR}" ${CLKS_SUBMODULE_CMAKE_ARGS}
    COMMAND ${CMAKE_COMMAND} --build "${CLKS_CMAKE_BUILD_DIR}" --target kernel
    USES_TERMINAL
)

add_custom_target(kernel-symbols
    COMMAND ${CMAKE_COMMAND} -S "${CLKS_SOURCE_DIR}" -B "${CLKS_CMAKE_BUILD_DIR}" ${CLKS_SUBMODULE_CMAKE_ARGS}
    COMMAND ${CMAKE_COMMAND} --build "${CLKS_CMAKE_BUILD_DIR}" --target kernel-symbols
    USES_TERMINAL
)
add_dependencies(kernel-symbols kernel)

if(CLEONOS_ENABLE)
    include("${CMAKE_SOURCE_DIR}/cleonos/CMakeLists.txt")
endif()

add_custom_target(setup-tools
    COMMAND ${CMAKE_COMMAND}
        "-DCLEONOS_ENABLE=${CLEONOS_ENABLE}"
        "-DGIT_TOOL=${GIT_TOOL}"
        "-DTAR_TOOL=${TAR}"
        "-DXORRISO_TOOL=${XORRISO}"
        "-DNO_COLOR=${NO_COLOR}"
        "-DCC_TOOL=${CC}"
        "-DLD_TOOL=${LD}"
        "-DOBJCOPY_TOOL=${OBJCOPY_FOR_TARGET}"
        "-DOBJDUMP_TOOL=${OBJDUMP_FOR_TARGET}"
        "-DREADELF_TOOL=${READELF_FOR_TARGET}"
        "-DNM_TOOL=${NM}"
        "-DADDR2LINE_TOOL=${ADDR2LINE}"
        "-DUSER_CC_TOOL=${USER_CC}"
        "-DUSER_LD_TOOL=${USER_LD}"
        "-DRUSTC_TOOL=${RUSTC}"
        "-DMAKE_TOOL=${MAKE_TOOL}"
        "-DSH_TOOL=${SH_TOOL}"
        -P "${CMAKE_SOURCE_DIR}/cmake/check_tools.cmake"
)

add_custom_target(setup-limine
    COMMAND ${CMAKE_COMMAND}
        "-DGIT_TOOL=${GIT_TOOL}"
        "-DMAKE_TOOL=${MAKE_TOOL}"
        "-DSH_TOOL=${SH_TOOL}"
        "-DNO_COLOR=${NO_COLOR}"
        "-DLIMINE_DIR=${LIMINE_DIR_ABS}"
        "-DLIMINE_REPO=${LIMINE_REPO}"
        "-DLIMINE_REF=${LIMINE_REF}"
        "-DLIMINE_BIN_DIR=${LIMINE_BIN_DIR_ABS}"
        "-DLIMINE_SETUP_STAMP=${LIMINE_SETUP_STAMP}"
        "-DLIMINE_BUILD_STAMP=${LIMINE_BUILD_STAMP}"
        "-DLIMINE_CONFIGURE_FLAGS=${LIMINE_CONFIGURE_FLAGS}"
        "-DLIMINE_SKIP_CONFIGURE=${LIMINE_SKIP_CONFIGURE}"
        "-DOBJCOPY_FOR_TARGET=${OBJCOPY_FOR_TARGET}"
        "-DOBJDUMP_FOR_TARGET=${OBJDUMP_FOR_TARGET}"
        "-DREADELF_FOR_TARGET=${READELF_FOR_TARGET}"
        -P "${CMAKE_SOURCE_DIR}/cmake/setup_limine.cmake"
)
add_dependencies(setup-limine setup-tools)

add_custom_target(setup DEPENDS setup-tools setup-limine)
add_custom_command(
    TARGET setup
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        "-DNO_COLOR=${NO_COLOR}"
        "-DLOG_LEVEL=INFO"
        "-DLOG_TEXT=environment ready"
        -P "${CL_LOG_EMIT_SCRIPT}"
)

if(CLEONOS_ENABLE)
    add_custom_target(disk-image
        COMMAND ${CMAKE_COMMAND}
            "-DDISK_IMAGE=${DISK_IMAGE}"
            "-DDISK_MB=${CLEONOS_DISK_IMAGE_MB}"
            -P "${CMAKE_SOURCE_DIR}/cmake/ensure_disk_image.cmake"
        BYPRODUCTS "${DISK_IMAGE}"
        VERBATIM
    )

    add_custom_command(
        OUTPUT "${ISO_IMAGE}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${ISO_ROOT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ISO_ROOT}/boot/limine" "${ISO_ROOT}/EFI/BOOT"
        COMMAND ${CMAKE_COMMAND} -E copy "${KERNEL_ELF}" "${ISO_ROOT}/boot/clks_kernel.elf"
        COMMAND ${CMAKE_COMMAND} -E copy "${RAMDISK_IMAGE}" "${ISO_ROOT}/boot/cleonos_ramdisk.tar"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/configs/limine.conf" "${ISO_ROOT}/boot/limine/limine.conf"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/configs/limine.conf" "${ISO_ROOT}/EFI/BOOT/limine.conf"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/configs/limine.conf" "${ISO_ROOT}/limine.conf"
        COMMAND ${CMAKE_COMMAND} -E copy "${LIMINE_BIN_DIR_ABS}/limine-bios.sys" "${ISO_ROOT}/boot/limine/limine-bios.sys"
        COMMAND ${CMAKE_COMMAND} -E copy "${LIMINE_BIN_DIR_ABS}/limine-bios-cd.bin" "${ISO_ROOT}/boot/limine/limine-bios-cd.bin"
        COMMAND ${CMAKE_COMMAND} -E copy "${LIMINE_BIN_DIR_ABS}/limine-uefi-cd.bin" "${ISO_ROOT}/boot/limine/limine-uefi-cd.bin"
        COMMAND ${CMAKE_COMMAND} -E copy "${LIMINE_BIN_DIR_ABS}/BOOTX64.EFI" "${ISO_ROOT}/EFI/BOOT/BOOTX64.EFI"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/build"
        COMMAND ${CMAKE_COMMAND}
            "-DNO_COLOR=${NO_COLOR}"
            "-DLOG_LEVEL=STEP"
            "-DLOG_TEXT=building iso -> ${ISO_IMAGE}"
            -P "${CL_LOG_EMIT_SCRIPT}"
        COMMAND ${XORRISO} -as mkisofs
            -b boot/limine/limine-bios-cd.bin
            -no-emul-boot
            -boot-load-size 4
            -boot-info-table
            --efi-boot boot/limine/limine-uefi-cd.bin
            -efi-boot-part
            --efi-boot-image
            --protective-msdos-label
            "${ISO_ROOT}"
            -o "${ISO_IMAGE}"
        COMMAND ${CMAKE_COMMAND}
            "-DNO_COLOR=${NO_COLOR}"
            "-DLOG_LEVEL=STEP"
            "-DLOG_TEXT=installing limine boot sectors"
            -P "${CL_LOG_EMIT_SCRIPT}"
        COMMAND "${LIMINE_BIN_DIR_ABS}/limine" bios-install "${ISO_IMAGE}"
        COMMAND ${CMAKE_COMMAND}
            "-DNO_COLOR=${NO_COLOR}"
            "-DLOG_LEVEL=INFO"
            "-DLOG_TEXT=iso ready: ${ISO_IMAGE}"
            -P "${CL_LOG_EMIT_SCRIPT}"
        DEPENDS "${KERNEL_ELF}" "${RAMDISK_IMAGE}" "${CMAKE_SOURCE_DIR}/configs/limine.conf"
        VERBATIM
    )

    add_custom_target(iso ALL DEPENDS "${ISO_IMAGE}")
    add_dependencies(iso setup-tools setup-limine kernel ramdisk)

    add_custom_target(run
        COMMAND ${CMAKE_COMMAND}
            "-DNO_COLOR=${NO_COLOR}"
            "-DLOG_LEVEL=STEP"
            "-DLOG_TEXT=launching qemu run"
            -P "${CL_LOG_EMIT_SCRIPT}"
        COMMAND ${QEMU_X86_64} -M pc -m 1024M -boot order=d -cdrom "${ISO_IMAGE}" -drive "file=${DISK_IMAGE},format=raw,if=ide,index=0,media=disk" -serial stdio
        DEPENDS iso disk-image
        USES_TERMINAL
    )

    add_custom_target(debug
        COMMAND ${CMAKE_COMMAND}
            "-DNO_COLOR=${NO_COLOR}"
            "-DLOG_LEVEL=STEP"
            "-DLOG_TEXT=launching qemu debug (-s -S)"
            -P "${CL_LOG_EMIT_SCRIPT}"
        COMMAND ${QEMU_X86_64} -M pc -m 1024M -boot order=d -cdrom "${ISO_IMAGE}" -drive "file=${DISK_IMAGE},format=raw,if=ide,index=0,media=disk" -serial stdio -s -S
        DEPENDS iso disk-image
        USES_TERMINAL
    )
else()
    add_custom_target(clks-default ALL DEPENDS kernel)
endif()

add_custom_target(clean-x86
    COMMAND ${CMAKE_COMMAND}
        "-DNO_COLOR=${NO_COLOR}"
        "-DLOG_LEVEL=STEP"
        "-DLOG_TEXT=cleaning ${BUILD_ROOT}"
        -P "${CL_LOG_EMIT_SCRIPT}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${BUILD_ROOT}"
    COMMAND ${CMAKE_COMMAND}
        "-DNO_COLOR=${NO_COLOR}"
        "-DLOG_LEVEL=INFO"
        "-DLOG_TEXT=clean done"
        -P "${CL_LOG_EMIT_SCRIPT}"
)

add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND}
        "-DNO_COLOR=${NO_COLOR}"
        "-DLOG_LEVEL=STEP"
        "-DLOG_TEXT=cleaning build"
        -P "${CL_LOG_EMIT_SCRIPT}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_SOURCE_DIR}/build"
    COMMAND ${CMAKE_COMMAND}
        "-DNO_COLOR=${NO_COLOR}"
        "-DLOG_LEVEL=INFO"
        "-DLOG_TEXT=clean-all done"
        -P "${CL_LOG_EMIT_SCRIPT}"
)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    if(CLEONOS_ENABLE)
        add_custom_target(menuconfig
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/menuconfig.py"
            USES_TERMINAL
        )
        add_custom_target(menuconfig-gui
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/menuconfig.py" --gui
            USES_TERMINAL
        )
    else()
        add_custom_target(menuconfig
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/menuconfig.py" --clks-only
            USES_TERMINAL
        )
        add_custom_target(menuconfig-gui
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/menuconfig.py" --gui --clks-only
            USES_TERMINAL
        )
    endif()
    add_custom_target(menuconfig-clks
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/menuconfig.py" --clks-only
        USES_TERMINAL
    )
    add_custom_target(menuconfig-gui-clks
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/menuconfig.py" --gui --clks-only
        USES_TERMINAL
    )
else()
    add_custom_target(menuconfig
        COMMAND ${CMAKE_COMMAND} -E echo "python3 not found; run scripts/menuconfig.py manually"
        USES_TERMINAL
    )
    add_custom_target(menuconfig-gui
        COMMAND ${CMAKE_COMMAND} -E echo "python3 not found; run scripts/menuconfig.py --gui manually"
        USES_TERMINAL
    )
    add_custom_target(menuconfig-clks
        COMMAND ${CMAKE_COMMAND} -E echo "python3 not found; run scripts/menuconfig.py --clks-only manually"
        USES_TERMINAL
    )
    add_custom_target(menuconfig-gui-clks
        COMMAND ${CMAKE_COMMAND} -E echo "python3 not found; run scripts/menuconfig.py --gui --clks-only manually"
        USES_TERMINAL
    )
endif()

if(CLEONOS_ENABLE)
    add_custom_target(cleonos-help
        COMMAND ${CMAKE_COMMAND} -E echo "CLeonOS CMake build system (x86_64 only)"
        COMMAND ${CMAKE_COMMAND} -E echo "  CLEONOS_ENABLE=${CLEONOS_ENABLE}"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake -S . -B build-cmake"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target menuconfig"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target menuconfig-gui"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target menuconfig-clks"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target menuconfig-gui-clks"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target setup"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target kernel"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target userapps"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target iso"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target run"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target debug"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target disk-image"
    )
else()
    add_custom_target(cleonos-help
        COMMAND ${CMAKE_COMMAND} -E echo "CLeonOS CMake build system (x86_64 only)"
        COMMAND ${CMAKE_COMMAND} -E echo "  CLEONOS_ENABLE=${CLEONOS_ENABLE}"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake -S . -B build-cmake"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target menuconfig"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target menuconfig-gui"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target menuconfig-clks"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target menuconfig-gui-clks"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target setup"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target kernel"
        COMMAND ${CMAKE_COMMAND} -E echo "  (CLKS-only mode: userapps/iso/run/debug/disk-image are disabled)"
    )
endif()

