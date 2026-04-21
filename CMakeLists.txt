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
resolve_tool_with_fallback(USER_CC cc gcc clang)
resolve_tool_with_fallback(USER_LD ld.lld ld)
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
set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/clks/arch/${CLKS_ARCH}/linker.ld")
set(USER_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/cleonos/c/user.ld")
set(KELF_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/cleonos/c/kelf.ld")
set(CLEONOS_MENUCONFIG_CMAKE "${CMAKE_SOURCE_DIR}/configs/menuconfig/config.cmake" CACHE FILEPATH "menuconfig generated CMake config")

if(EXISTS "${CLEONOS_MENUCONFIG_CMAKE}")
    include("${CLEONOS_MENUCONFIG_CMAKE}")
    cl_log_info("menuconfig loaded from ${CLEONOS_MENUCONFIG_CMAKE}")
endif()

function(cl_set_bool_cache VAR_NAME DEFAULT_VALUE DOC_TEXT)
    set(_raw_value "")
    set(_has_value OFF)

    if(DEFINED ${VAR_NAME})
        set(_raw_value "${${VAR_NAME}}")
        set(_has_value ON)
    elseif(DEFINED ${VAR_NAME}_IS_ENABLED)
        set(_raw_value "${${VAR_NAME}_IS_ENABLED}")
        set(_has_value ON)
    endif()

    if(NOT _has_value)
        set(${VAR_NAME} ${DEFAULT_VALUE} CACHE BOOL "${DOC_TEXT}")
    else()
        string(TOUPPER "${_raw_value}" _raw_upper)
        if(_raw_upper STREQUAL "Y" OR _raw_upper STREQUAL "M" OR _raw_upper STREQUAL "ON" OR _raw_upper STREQUAL "TRUE" OR _raw_upper STREQUAL "1")
            set(_bool_value ON)
        else()
            set(_bool_value OFF)
        endif()
        set(${VAR_NAME} ${_bool_value} CACHE BOOL "${DOC_TEXT}" FORCE)
    endif()
endfunction()

function(cl_bool_to_int BOOL_VAR OUT_VAR)
    if(${BOOL_VAR})
        set(${OUT_VAR} 1 PARENT_SCOPE)
    else()
        set(${OUT_VAR} 0 PARENT_SCOPE)
    endif()
endfunction()

cl_set_bool_cache(CLEONOS_CLKS_ENABLE_AUDIO ON "Initialize kernel audio subsystem")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_MOUSE ON "Initialize kernel mouse subsystem")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_DESKTOP ON "Initialize desktop compositor subsystem")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_DRIVER_MANAGER ON "Initialize kernel driver manager")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_KELF ON "Enable KELF app dispatcher")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USERLAND_AUTO_EXEC ON "Auto exec /shell/shell.elf on boot")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_HEAP_SELFTEST ON "Run heap selftest at boot")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_EXTERNAL_PSF ON "Load /system/tty.psf external font")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_KEYBOARD ON "Initialize keyboard input subsystem")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_ELFRUNNER_PROBE ON "Run ELFRUNNER kernel ELF probe")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_KLOGD_TASK ON "Enable scheduler klogd task")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_KWORKER_TASK ON "Enable scheduler kworker task")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USRD_TASK ON "Enable scheduler usrd task")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_BOOT_VIDEO_LOG ON "Print boot framebuffer geometry logs")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_PMM_STATS_LOG ON "Print PMM stats logs")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_HEAP_STATS_LOG ON "Print heap stats logs")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_FS_ROOT_LOG ON "Print root children FS log")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_SYSTEM_DIR_CHECK ON "Require /system directory sanity check")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_ELFRUNNER_INIT ON "Initialize ELFRUNNER framework")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_SYSCALL_TICK_QUERY ON "Query syscall timer ticks at boot")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_TTY_READY_LOG ON "Print TTY ready logs")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_IDLE_DEBUG_LOG ON "Print idle loop debug log")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_PROCFS ON "Enable virtual /proc procfs syscall layer")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_EXEC_SERIAL_LOG ON "Enable EXEC info logs on serial output")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USC ON "Enable UserSafeController dangerous syscall confirmations")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USC_SC_FS_MKDIR ON "USC intercept policy: FS_MKDIR")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USC_SC_FS_WRITE ON "USC intercept policy: FS_WRITE")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USC_SC_FS_APPEND ON "USC intercept policy: FS_APPEND")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USC_SC_FS_REMOVE ON "USC intercept policy: FS_REMOVE")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USC_SC_EXEC_PATH ON "USC intercept policy: EXEC_PATH")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USC_SC_EXEC_PATHV ON "USC intercept policy: EXEC_PATHV")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USC_SC_EXEC_PATHV_IO ON "USC intercept policy: EXEC_PATHV_IO")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USC_SC_SPAWN_PATH ON "USC intercept policy: SPAWN_PATH")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USC_SC_SPAWN_PATHV ON "USC intercept policy: SPAWN_PATHV")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USC_SC_PROC_KILL ON "USC intercept policy: PROC_KILL")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USC_SC_SHUTDOWN ON "USC intercept policy: SHUTDOWN")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USC_SC_RESTART ON "USC intercept policy: RESTART")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_KBD_TTY_SWITCH_HOTKEY ON "Enable ALT+F1..F4 TTY switch hotkey")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_KBD_CTRL_SHORTCUTS ON "Enable Ctrl+A/C/V keyboard shortcuts")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_KBD_FORCE_STOP_HOTKEY ON "Enable Ctrl+Alt+C force-stop hotkey")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USER_INIT_SCRIPT_PROBE ON "Probe /shell/init.cmd during userland init")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USER_SYSTEM_APP_PROBE ON "Probe /system apps (elfrunner/memc) during userland init")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_SCHED_TASK_COUNT_LOG ON "Print scheduler task count during boot")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_INTERRUPT_READY_LOG ON "Print interrupt ready log after IDT/PIC init")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_SHELL_MODE_LOG ON "Print default shell mode log during boot")

cl_bool_to_int(CLEONOS_CLKS_ENABLE_AUDIO CLKS_CFG_AUDIO_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_MOUSE CLKS_CFG_MOUSE_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_DESKTOP CLKS_CFG_DESKTOP_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_DRIVER_MANAGER CLKS_CFG_DRIVER_MANAGER_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_KELF CLKS_CFG_KELF_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USERLAND_AUTO_EXEC CLKS_CFG_USERLAND_AUTO_EXEC_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_HEAP_SELFTEST CLKS_CFG_HEAP_SELFTEST_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_EXTERNAL_PSF CLKS_CFG_EXTERNAL_PSF_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_KEYBOARD CLKS_CFG_KEYBOARD_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_ELFRUNNER_PROBE CLKS_CFG_ELFRUNNER_PROBE_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_KLOGD_TASK CLKS_CFG_KLOGD_TASK_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_KWORKER_TASK CLKS_CFG_KWORKER_TASK_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USRD_TASK CLKS_CFG_USRD_TASK_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_BOOT_VIDEO_LOG CLKS_CFG_BOOT_VIDEO_LOG_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_PMM_STATS_LOG CLKS_CFG_PMM_STATS_LOG_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_HEAP_STATS_LOG CLKS_CFG_HEAP_STATS_LOG_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_FS_ROOT_LOG CLKS_CFG_FS_ROOT_LOG_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_SYSTEM_DIR_CHECK CLKS_CFG_SYSTEM_DIR_CHECK_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_ELFRUNNER_INIT CLKS_CFG_ELFRUNNER_INIT_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_SYSCALL_TICK_QUERY CLKS_CFG_SYSCALL_TICK_QUERY_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_TTY_READY_LOG CLKS_CFG_TTY_READY_LOG_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_IDLE_DEBUG_LOG CLKS_CFG_IDLE_DEBUG_LOG_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_PROCFS CLKS_CFG_PROCFS_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_EXEC_SERIAL_LOG CLKS_CFG_EXEC_SERIAL_LOG_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USC CLKS_CFG_USC_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USC_SC_FS_MKDIR CLKS_CFG_USC_SC_FS_MKDIR_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USC_SC_FS_WRITE CLKS_CFG_USC_SC_FS_WRITE_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USC_SC_FS_APPEND CLKS_CFG_USC_SC_FS_APPEND_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USC_SC_FS_REMOVE CLKS_CFG_USC_SC_FS_REMOVE_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USC_SC_EXEC_PATH CLKS_CFG_USC_SC_EXEC_PATH_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USC_SC_EXEC_PATHV CLKS_CFG_USC_SC_EXEC_PATHV_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USC_SC_EXEC_PATHV_IO CLKS_CFG_USC_SC_EXEC_PATHV_IO_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USC_SC_SPAWN_PATH CLKS_CFG_USC_SC_SPAWN_PATH_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USC_SC_SPAWN_PATHV CLKS_CFG_USC_SC_SPAWN_PATHV_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USC_SC_PROC_KILL CLKS_CFG_USC_SC_PROC_KILL_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USC_SC_SHUTDOWN CLKS_CFG_USC_SC_SHUTDOWN_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USC_SC_RESTART CLKS_CFG_USC_SC_RESTART_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_KBD_TTY_SWITCH_HOTKEY CLKS_CFG_KBD_TTY_SWITCH_HOTKEY_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_KBD_CTRL_SHORTCUTS CLKS_CFG_KBD_CTRL_SHORTCUTS_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_KBD_FORCE_STOP_HOTKEY CLKS_CFG_KBD_FORCE_STOP_HOTKEY_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USER_INIT_SCRIPT_PROBE CLKS_CFG_USER_INIT_SCRIPT_PROBE_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USER_SYSTEM_APP_PROBE CLKS_CFG_USER_SYSTEM_APP_PROBE_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_SCHED_TASK_COUNT_LOG CLKS_CFG_SCHED_TASK_COUNT_LOG_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_INTERRUPT_READY_LOG CLKS_CFG_INTERRUPT_READY_LOG_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_SHELL_MODE_LOG CLKS_CFG_SHELL_MODE_LOG_INT)

set(CFLAGS_COMMON
    -std=c11
    -ffreestanding
    -fno-stack-protector
    -fno-builtin
    -g
    -Wall
    -Wextra
    -Werror
    "-I${CMAKE_SOURCE_DIR}/clks/include"
    "-I${CMAKE_SOURCE_DIR}/common/include"
)

set(CXXFLAGS_COMMON
    -std=c++17
    -ffreestanding
    -fno-stack-protector
    -fno-builtin
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
    -fno-use-cxa-atexit
    -g
    -Wall
    -Wextra
    -Werror
    "-I${CMAKE_SOURCE_DIR}/clks/include"
    "-I${CMAKE_SOURCE_DIR}/common/include"
)

set(ARCH_CFLAGS
    -DCLKS_ARCH_X86_64=1
    "-DCLKS_CFG_AUDIO=${CLKS_CFG_AUDIO_INT}"
    "-DCLKS_CFG_MOUSE=${CLKS_CFG_MOUSE_INT}"
    "-DCLKS_CFG_DESKTOP=${CLKS_CFG_DESKTOP_INT}"
    "-DCLKS_CFG_DRIVER_MANAGER=${CLKS_CFG_DRIVER_MANAGER_INT}"
    "-DCLKS_CFG_KELF=${CLKS_CFG_KELF_INT}"
    "-DCLKS_CFG_USERLAND_AUTO_EXEC=${CLKS_CFG_USERLAND_AUTO_EXEC_INT}"
    "-DCLKS_CFG_HEAP_SELFTEST=${CLKS_CFG_HEAP_SELFTEST_INT}"
    "-DCLKS_CFG_EXTERNAL_PSF=${CLKS_CFG_EXTERNAL_PSF_INT}"
    "-DCLKS_CFG_KEYBOARD=${CLKS_CFG_KEYBOARD_INT}"
    "-DCLKS_CFG_ELFRUNNER_PROBE=${CLKS_CFG_ELFRUNNER_PROBE_INT}"
    "-DCLKS_CFG_KLOGD_TASK=${CLKS_CFG_KLOGD_TASK_INT}"
    "-DCLKS_CFG_KWORKER_TASK=${CLKS_CFG_KWORKER_TASK_INT}"
    "-DCLKS_CFG_USRD_TASK=${CLKS_CFG_USRD_TASK_INT}"
    "-DCLKS_CFG_BOOT_VIDEO_LOG=${CLKS_CFG_BOOT_VIDEO_LOG_INT}"
    "-DCLKS_CFG_PMM_STATS_LOG=${CLKS_CFG_PMM_STATS_LOG_INT}"
    "-DCLKS_CFG_HEAP_STATS_LOG=${CLKS_CFG_HEAP_STATS_LOG_INT}"
    "-DCLKS_CFG_FS_ROOT_LOG=${CLKS_CFG_FS_ROOT_LOG_INT}"
    "-DCLKS_CFG_SYSTEM_DIR_CHECK=${CLKS_CFG_SYSTEM_DIR_CHECK_INT}"
    "-DCLKS_CFG_ELFRUNNER_INIT=${CLKS_CFG_ELFRUNNER_INIT_INT}"
    "-DCLKS_CFG_SYSCALL_TICK_QUERY=${CLKS_CFG_SYSCALL_TICK_QUERY_INT}"
    "-DCLKS_CFG_TTY_READY_LOG=${CLKS_CFG_TTY_READY_LOG_INT}"
    "-DCLKS_CFG_IDLE_DEBUG_LOG=${CLKS_CFG_IDLE_DEBUG_LOG_INT}"
    "-DCLKS_CFG_PROCFS=${CLKS_CFG_PROCFS_INT}"
    "-DCLKS_CFG_EXEC_SERIAL_LOG=${CLKS_CFG_EXEC_SERIAL_LOG_INT}"
    "-DCLKS_CFG_USC=${CLKS_CFG_USC_INT}"
    "-DCLKS_CFG_USC_SC_FS_MKDIR=${CLKS_CFG_USC_SC_FS_MKDIR_INT}"
    "-DCLKS_CFG_USC_SC_FS_WRITE=${CLKS_CFG_USC_SC_FS_WRITE_INT}"
    "-DCLKS_CFG_USC_SC_FS_APPEND=${CLKS_CFG_USC_SC_FS_APPEND_INT}"
    "-DCLKS_CFG_USC_SC_FS_REMOVE=${CLKS_CFG_USC_SC_FS_REMOVE_INT}"
    "-DCLKS_CFG_USC_SC_EXEC_PATH=${CLKS_CFG_USC_SC_EXEC_PATH_INT}"
    "-DCLKS_CFG_USC_SC_EXEC_PATHV=${CLKS_CFG_USC_SC_EXEC_PATHV_INT}"
    "-DCLKS_CFG_USC_SC_EXEC_PATHV_IO=${CLKS_CFG_USC_SC_EXEC_PATHV_IO_INT}"
    "-DCLKS_CFG_USC_SC_SPAWN_PATH=${CLKS_CFG_USC_SC_SPAWN_PATH_INT}"
    "-DCLKS_CFG_USC_SC_SPAWN_PATHV=${CLKS_CFG_USC_SC_SPAWN_PATHV_INT}"
    "-DCLKS_CFG_USC_SC_PROC_KILL=${CLKS_CFG_USC_SC_PROC_KILL_INT}"
    "-DCLKS_CFG_USC_SC_SHUTDOWN=${CLKS_CFG_USC_SC_SHUTDOWN_INT}"
    "-DCLKS_CFG_USC_SC_RESTART=${CLKS_CFG_USC_SC_RESTART_INT}"
    "-DCLKS_CFG_KBD_TTY_SWITCH_HOTKEY=${CLKS_CFG_KBD_TTY_SWITCH_HOTKEY_INT}"
    "-DCLKS_CFG_KBD_CTRL_SHORTCUTS=${CLKS_CFG_KBD_CTRL_SHORTCUTS_INT}"
    "-DCLKS_CFG_KBD_FORCE_STOP_HOTKEY=${CLKS_CFG_KBD_FORCE_STOP_HOTKEY_INT}"
    "-DCLKS_CFG_USER_INIT_SCRIPT_PROBE=${CLKS_CFG_USER_INIT_SCRIPT_PROBE_INT}"
    "-DCLKS_CFG_USER_SYSTEM_APP_PROBE=${CLKS_CFG_USER_SYSTEM_APP_PROBE_INT}"
    "-DCLKS_CFG_SCHED_TASK_COUNT_LOG=${CLKS_CFG_SCHED_TASK_COUNT_LOG_INT}"
    "-DCLKS_CFG_INTERRUPT_READY_LOG=${CLKS_CFG_INTERRUPT_READY_LOG_INT}"
    "-DCLKS_CFG_SHELL_MODE_LOG=${CLKS_CFG_SHELL_MODE_LOG_INT}"
    -m64
    -mno-red-zone
    -mcmodel=kernel
    -fno-pic
    -fno-pie
)

set(ASFLAGS_COMMON
    -ffreestanding
    "-I${CMAKE_SOURCE_DIR}/clks/include"
)

set(LDFLAGS_COMMON
    -nostdlib
    -z
    max-page-size=0x1000
)

set(USER_CFLAGS
    -std=c11
    -ffreestanding
    -fno-stack-protector
    -fno-builtin
    -Wall
    -Wextra
    -Werror
    "-I${CMAKE_SOURCE_DIR}/cleonos/c/include"
    "-I${CMAKE_SOURCE_DIR}/common/include"
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
    "-I${CMAKE_SOURCE_DIR}/common/include"
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

include("${CMAKE_SOURCE_DIR}/clks/CMakeLists.txt")
include("${CMAKE_SOURCE_DIR}/cleonos/CMakeLists.txt")

add_custom_target(setup-tools
    COMMAND ${CMAKE_COMMAND}
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
    COMMAND ${QEMU_X86_64} -M q35 -m 1024M -cdrom "${ISO_IMAGE}" -serial stdio
    DEPENDS iso
    USES_TERMINAL
)

add_custom_target(debug
    COMMAND ${CMAKE_COMMAND}
        "-DNO_COLOR=${NO_COLOR}"
        "-DLOG_LEVEL=STEP"
        "-DLOG_TEXT=launching qemu debug (-s -S)"
        -P "${CL_LOG_EMIT_SCRIPT}"
    COMMAND ${QEMU_X86_64} -M q35 -m 1024M -cdrom "${ISO_IMAGE}" -serial stdio -s -S
    DEPENDS iso
    USES_TERMINAL
)

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
        COMMAND ${CMAKE_COMMAND} -E echo "python3 not found; run scripts/menuconfig.py manually"
        USES_TERMINAL
    )
    add_custom_target(menuconfig-gui
        COMMAND ${CMAKE_COMMAND} -E echo "python3 not found; run scripts/menuconfig.py --gui manually"
        USES_TERMINAL
    )
endif()

add_custom_target(cleonos-help
    COMMAND ${CMAKE_COMMAND} -E echo "CLeonOS CMake build system (x86_64 only)"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake -S . -B build-cmake"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target menuconfig"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target menuconfig-gui"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target setup"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target userapps"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target iso"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target run"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target debug"
)

