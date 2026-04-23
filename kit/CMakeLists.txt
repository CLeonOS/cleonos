cmake_minimum_required(VERSION 3.20)
project(CLeonOSKit NONE)

set(CLEONOS_KIT_APPS_DIR "${CMAKE_SOURCE_DIR}/apps" CACHE PATH "Directory containing app subdirectories")
set(CLEONOS_KIT_RUNTIME_DIR "${CMAKE_SOURCE_DIR}/runtime" CACHE PATH "Directory containing user runtime sources")
set(CLEONOS_KIT_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include" CACHE PATH "Directory containing user runtime headers")
set(CLEONOS_KIT_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/linker/user.ld" CACHE FILEPATH "Linker script for user ELF")
set(CLEONOS_KIT_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build/apps" CACHE PATH "Output directory for generated ELF files")
set(CLEONOS_KIT_OBJ_DIR "${CMAKE_SOURCE_DIR}/build/obj" CACHE PATH "Output directory for object files")
set(CLEONOS_KIT_APP_NAMES "" CACHE STRING "Optional app list, separated by comma/semicolon (empty means all apps)")
set(CLEONOS_KIT_USE_MAIN_ADAPTER ON CACHE BOOL "When ON, apps can provide plain main(int,char**)")
set(CLEONOS_KIT_ENABLE_WERROR ON CACHE BOOL "Treat warnings as errors")

set(CLEONOS_KIT_CC "x86_64-elf-gcc" CACHE STRING "C compiler for user ELF objects")
set(CLEONOS_KIT_LD "x86_64-elf-ld" CACHE STRING "Linker for user ELF")

function(resolve_tool_with_fallback VAR_NAME)
    set(_fallbacks ${ARGN})
    set(_requested "${${VAR_NAME}}")

    if("${_requested}" STREQUAL "")
        message(FATAL_ERROR "empty tool variable: ${VAR_NAME}")
    endif()

    if(IS_ABSOLUTE "${_requested}")
        if(NOT EXISTS "${_requested}")
            message(FATAL_ERROR "${VAR_NAME} not found: ${_requested}")
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
                    message(STATUS "${VAR_NAME} '${_requested}' not found; fallback to '${_cand}'")
                    set(_resolved "${_cand_path}")
                    break()
                endif()
            endforeach()
            if("${_resolved}" STREQUAL "")
                message(FATAL_ERROR "${VAR_NAME} tool not found: '${_requested}', fallbacks='${_fallbacks}'")
            endif()
        endif()
    endif()

    set(${VAR_NAME} "${_resolved}" CACHE STRING "resolved tool path" FORCE)
    set(${VAR_NAME} "${_resolved}" PARENT_SCOPE)
endfunction()

resolve_tool_with_fallback(CLEONOS_KIT_CC gcc clang cc)
resolve_tool_with_fallback(CLEONOS_KIT_LD ld ld.lld)

execute_process(
    COMMAND ${CLEONOS_KIT_LD} -V
    RESULT_VARIABLE _ld_probe_rc
    OUTPUT_VARIABLE _ld_probe_out
    ERROR_VARIABLE _ld_probe_err
)

execute_process(
    COMMAND ${CLEONOS_KIT_LD} --help
    RESULT_VARIABLE _ld_help_rc
    OUTPUT_VARIABLE _ld_help_out
    ERROR_VARIABLE _ld_help_err
)

set(_ld_probe_text "${_ld_probe_out}\n${_ld_probe_err}\n${_ld_help_out}\n${_ld_help_err}")
string(TOLOWER "${_ld_probe_text}" _ld_probe_lower)

if(_ld_probe_lower MATCHES "i386pep"
   OR _ld_probe_lower MATCHES "pei-"
   OR _ld_probe_lower MATCHES "pe-coff"
   OR _ld_probe_lower MATCHES "appcontainer"
   OR _ld_probe_lower MATCHES "auto-import")
    message(FATAL_ERROR
        "selected linker appears to target PE/COFF, not ELF. "
        "Use an ELF toolchain, for example: "
        "-DCLEONOS_KIT_CC=x86_64-elf-gcc -DCLEONOS_KIT_LD=x86_64-elf-ld")
endif()

if(NOT EXISTS "${CLEONOS_KIT_LINKER_SCRIPT}")
    message(FATAL_ERROR "missing linker script: ${CLEONOS_KIT_LINKER_SCRIPT}")
endif()

if(NOT IS_DIRECTORY "${CLEONOS_KIT_APPS_DIR}")
    message(FATAL_ERROR "missing apps directory: ${CLEONOS_KIT_APPS_DIR}")
endif()

if(NOT IS_DIRECTORY "${CLEONOS_KIT_RUNTIME_DIR}")
    message(FATAL_ERROR "missing runtime directory: ${CLEONOS_KIT_RUNTIME_DIR}")
endif()

if(NOT IS_DIRECTORY "${CLEONOS_KIT_INCLUDE_DIR}")
    message(FATAL_ERROR "missing include directory: ${CLEONOS_KIT_INCLUDE_DIR}")
endif()

file(MAKE_DIRECTORY "${CLEONOS_KIT_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${CLEONOS_KIT_OBJ_DIR}")

set(_runtime_sources
    "${CLEONOS_KIT_RUNTIME_DIR}/runtime.c"
    "${CLEONOS_KIT_RUNTIME_DIR}/syscall.c"
    "${CLEONOS_KIT_RUNTIME_DIR}/stdio.c"
    "${CLEONOS_KIT_RUNTIME_DIR}/libc_string.c"
    "${CLEONOS_KIT_RUNTIME_DIR}/libc_stdlib.c"
    "${CLEONOS_KIT_RUNTIME_DIR}/libc_ctype.c"
    "${CLEONOS_KIT_RUNTIME_DIR}/dlfcn.c"
)

if(CLEONOS_KIT_USE_MAIN_ADAPTER)
    list(APPEND _runtime_sources "${CLEONOS_KIT_RUNTIME_DIR}/main_adapter.c")
endif()

foreach(_src IN LISTS _runtime_sources)
    if(NOT EXISTS "${_src}")
        message(FATAL_ERROR "missing runtime source: ${_src}")
    endif()
endforeach()

set(CLEONOS_KIT_CFLAGS
    -std=c11
    -ffreestanding
    -fno-stack-protector
    -fno-builtin
    -Wall
    -Wextra
    -m64
    -mno-red-zone
    -fno-pic
    -fno-pie
    "-I${CLEONOS_KIT_INCLUDE_DIR}"
)

if(CLEONOS_KIT_ENABLE_WERROR)
    list(APPEND CLEONOS_KIT_CFLAGS -Werror)
endif()

set(CLEONOS_KIT_LDFLAGS "" CACHE STRING "Extra linker flags (semicolon separated)")

function(cleonos_compile_object SRC_PATH OUT_VAR)
    file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}" "${SRC_PATH}")
    string(REGEX REPLACE "\\.c$" ".o" _obj_rel "${_rel}")
    set(_obj_path "${CLEONOS_KIT_OBJ_DIR}/${_obj_rel}")
    get_filename_component(_obj_dir "${_obj_path}" DIRECTORY)

    add_custom_command(
        OUTPUT "${_obj_path}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_obj_dir}"
        COMMAND ${CLEONOS_KIT_CC} ${CLEONOS_KIT_CFLAGS} -c "${SRC_PATH}" -o "${_obj_path}"
        DEPENDS "${SRC_PATH}"
        VERBATIM
    )

    set(${OUT_VAR} "${_obj_path}" PARENT_SCOPE)
endfunction()

function(cleonos_kit_add_app APP_NAME)
    set(_app_sources ${ARGN})

    if(NOT _app_sources)
        message(FATAL_ERROR "cleonos_kit_add_app(${APP_NAME}) requires at least one source")
    endif()

    set(_obj_list)
    foreach(_src IN LISTS _runtime_sources _app_sources)
        cleonos_compile_object("${_src}" _obj)
        list(APPEND _obj_list "${_obj}")
    endforeach()

    set(_out_path "${CLEONOS_KIT_OUTPUT_DIR}/${APP_NAME}.elf")
    add_custom_command(
        OUTPUT "${_out_path}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CLEONOS_KIT_OUTPUT_DIR}"
        COMMAND ${CLEONOS_KIT_LD} ${CLEONOS_KIT_LDFLAGS} -T "${CLEONOS_KIT_LINKER_SCRIPT}" -o "${_out_path}" ${_obj_list}
        DEPENDS ${_obj_list} "${CLEONOS_KIT_LINKER_SCRIPT}"
        VERBATIM
    )

    add_custom_target("app-${APP_NAME}" DEPENDS "${_out_path}")
    set_property(GLOBAL APPEND PROPERTY CLEONOS_KIT_APP_TARGETS "app-${APP_NAME}")
    set_property(GLOBAL APPEND PROPERTY CLEONOS_KIT_APP_OUTPUTS "${_out_path}")
endfunction()

set(_app_names)
if("${CLEONOS_KIT_APP_NAMES}" STREQUAL "")
    file(GLOB _app_entries RELATIVE "${CLEONOS_KIT_APPS_DIR}" "${CLEONOS_KIT_APPS_DIR}/*")
    foreach(_entry IN LISTS _app_entries)
        if(IS_DIRECTORY "${CLEONOS_KIT_APPS_DIR}/${_entry}")
            list(APPEND _app_names "${_entry}")
        endif()
    endforeach()
else()
    string(REPLACE "," ";" _app_names "${CLEONOS_KIT_APP_NAMES}")
endif()

list(REMOVE_DUPLICATES _app_names)
list(SORT _app_names)

if(NOT _app_names)
    message(FATAL_ERROR "no apps selected/found under ${CLEONOS_KIT_APPS_DIR}")
endif()

foreach(_app IN LISTS _app_names)
    if(NOT IS_DIRECTORY "${CLEONOS_KIT_APPS_DIR}/${_app}")
        message(FATAL_ERROR "app directory not found: ${CLEONOS_KIT_APPS_DIR}/${_app}")
    endif()

    file(GLOB_RECURSE _app_sources CONFIGURE_DEPENDS "${CLEONOS_KIT_APPS_DIR}/${_app}/*.c")
    if(NOT _app_sources)
        message(FATAL_ERROR "no C sources found for app '${_app}' in ${CLEONOS_KIT_APPS_DIR}/${_app}")
    endif()

    cleonos_kit_add_app("${_app}" ${_app_sources})
endforeach()

get_property(_kit_targets GLOBAL PROPERTY CLEONOS_KIT_APP_TARGETS)
get_property(_kit_outputs GLOBAL PROPERTY CLEONOS_KIT_APP_OUTPUTS)

if(NOT _kit_targets)
    message(FATAL_ERROR "no app targets were generated")
endif()

add_custom_target(apps ALL DEPENDS ${_kit_targets})

message(STATUS "CLeonOS kit apps: ${_app_names}")
message(STATUS "CLeonOS kit output: ${CLEONOS_KIT_OUTPUT_DIR}")
