file(GLOB_RECURSE USER_INC_SOURCES_ABS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/cleonos/**/*.inc"
)
list(SORT USER_INC_SOURCES_ABS)

file(GLOB_RECURSE USER_COMMON_SOURCES_ABS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/cleonos/c/src/*.c")
set(USER_COMMON_SOURCES)
foreach(_abs IN LISTS USER_COMMON_SOURCES_ABS)
    file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}" "${_abs}")
    list(APPEND USER_COMMON_SOURCES "${_rel}")
endforeach()
list(SORT USER_COMMON_SOURCES)

file(GLOB USER_APP_MAIN_SOURCES_ABS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/cleonos/c/apps/*_main.c")
file(GLOB USER_APP_KMAIN_SOURCES_ABS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/cleonos/c/apps/*_kmain.c")

set(USER_APP_MAIN_SOURCES)
set(USER_APP_KMAIN_SOURCES)
foreach(_abs IN LISTS USER_APP_MAIN_SOURCES_ABS)
    file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}" "${_abs}")
    list(APPEND USER_APP_MAIN_SOURCES "${_rel}")
endforeach()
foreach(_abs IN LISTS USER_APP_KMAIN_SOURCES_ABS)
    file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}" "${_abs}")
    list(APPEND USER_APP_KMAIN_SOURCES "${_rel}")
endforeach()
list(SORT USER_APP_MAIN_SOURCES)
list(SORT USER_APP_KMAIN_SOURCES)

if(NOT USER_COMMON_SOURCES)
    cl_log_error("no user common sources found in cleonos/c/src/")
endif()
if(NOT USER_APP_MAIN_SOURCES)
    cl_log_error("no user app *_main.c found in cleonos/c/apps/")
endif()
if(NOT USER_APP_KMAIN_SOURCES)
    cl_log_error("no kernel app *_kmain.c found in cleonos/c/apps/")
endif()

function(add_user_c_object SRC OUTPUT_VAR)
    string(REGEX REPLACE "\\.c$" ".o" OBJ_REL "${SRC}")
    set(OBJ_PATH "${USER_OBJ_ROOT}/${OBJ_REL}")
    get_filename_component(OBJ_DIR "${OBJ_PATH}" DIRECTORY)
    set(_deps "${CMAKE_SOURCE_DIR}/${SRC}" ${USER_INC_SOURCES_ABS})

    add_custom_command(
        OUTPUT "${OBJ_PATH}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OBJ_DIR}"
        COMMAND ${USER_CC} ${USER_CFLAGS} -c "${CMAKE_SOURCE_DIR}/${SRC}" -o "${OBJ_PATH}"
        DEPENDS ${_deps}
        VERBATIM
    )

    set(${OUTPUT_VAR} "${OBJ_PATH}" PARENT_SCOPE)
endfunction()

function(add_user_c_object_doom SRC OUTPUT_VAR)
    string(REGEX REPLACE "\\.c$" ".o" OBJ_REL "${SRC}")
    set(OBJ_PATH "${USER_OBJ_ROOT}/${OBJ_REL}")
    get_filename_component(OBJ_DIR "${OBJ_PATH}" DIRECTORY)
    set(_deps "${CMAKE_SOURCE_DIR}/${SRC}" ${USER_INC_SOURCES_ABS} ${DOOMGENERIC_DEP_SOURCES_ABS})

    add_custom_command(
        OUTPUT "${OBJ_PATH}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OBJ_DIR}"
        COMMAND ${USER_CC} ${USER_CFLAGS_DOOM} -c "${CMAKE_SOURCE_DIR}/${SRC}" -o "${OBJ_PATH}"
        DEPENDS ${_deps}
        VERBATIM
    )

    set(${OUTPUT_VAR} "${OBJ_PATH}" PARENT_SCOPE)
endfunction()

set(USER_COMMON_OBJECTS)
foreach(SRC IN LISTS USER_COMMON_SOURCES)
    add_user_c_object("${SRC}" OBJ_OUT)
    list(APPEND USER_COMMON_OBJECTS "${OBJ_OUT}")
endforeach()

add_custom_command(
    OUTPUT "${USER_RUST_LIB}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${USER_LIB_DIR}"
    COMMAND ${RUSTC} --crate-type staticlib -C panic=abort -O "${CMAKE_SOURCE_DIR}/cleonos/rust/src/lib.rs" -o "${USER_RUST_LIB}"
    DEPENDS "${CMAKE_SOURCE_DIR}/cleonos/rust/src/lib.rs"
    VERBATIM
)

set(USER_KELF_APP_NAMES)
foreach(SRC IN LISTS USER_APP_KMAIN_SOURCES)
    get_filename_component(_stem "${SRC}" NAME_WE)
    string(REGEX REPLACE "_kmain$" "" _app_name "${_stem}")
    list(APPEND USER_KELF_APP_NAMES "${_app_name}")
endforeach()
list(REMOVE_DUPLICATES USER_KELF_APP_NAMES)

set(USER_APP_OUTPUTS)
set(USER_APP_NAMES)
set(RAMDISK_SHELL_APPS)
set(RAMDISK_DRIVER_APPS)
set(RAMDISK_SYSTEM_APPS)
set(RAMDISK_ROOT_APPS)

set(DOOMGENERIC_DIR "${CMAKE_SOURCE_DIR}/cleonos/third-party/doomgeneric/doomgeneric")
set(DOOMGENERIC_SRC_BASENAMES
    dummy am_map doomdef doomstat dstrings d_event d_items d_iwad d_loop d_main d_mode d_net f_finale f_wipe
    g_game hu_lib hu_stuff info i_cdmus i_endoom i_joystick i_scale i_sound i_system i_timer memio m_argv
    m_bbox m_cheat m_config m_controls m_fixed m_menu m_misc m_random p_ceilng p_doors p_enemy p_floor
    p_inter p_lights p_map p_maputl p_mobj p_plats p_pspr p_saveg p_setup p_sight p_spec p_switch p_telept
    p_tick p_user r_bsp r_data r_draw r_main r_plane r_segs r_sky r_things sha1 sounds statdump st_lib st_stuff
    s_sound tables v_video wi_stuff w_checksum w_file w_main w_wad z_zone w_file_stdc i_input i_video doomgeneric
)
set(DOOMGENERIC_APP_SOURCES)
foreach(_base IN LISTS DOOMGENERIC_SRC_BASENAMES)
    list(APPEND DOOMGENERIC_APP_SOURCES "cleonos/third-party/doomgeneric/doomgeneric/${_base}.c")
endforeach()
set(DOOMGENERIC_REQUIRED_SOURCE "${CMAKE_SOURCE_DIR}/cleonos/third-party/doomgeneric/doomgeneric/dummy.c")
if(EXISTS "${DOOMGENERIC_REQUIRED_SOURCE}")
    set(DOOMGENERIC_AVAILABLE ON)
else()
    set(DOOMGENERIC_AVAILABLE OFF)
    cl_log_warn("doomgeneric sources missing, doom app will be skipped: ${DOOMGENERIC_REQUIRED_SOURCE}")
endif()
file(GLOB_RECURSE DOOMGENERIC_DEP_SOURCES_ABS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/cleonos/third-party/doomgeneric/doomgeneric/*.h"
    "${CMAKE_SOURCE_DIR}/cleonos/c/apps/doom/*.h"
)
list(SORT DOOMGENERIC_DEP_SOURCES_ABS)

set(USER_SHELL_COMMAND_APPS
    help args ls cat grep head tail wc cut uniq sort pwd cd exec pid spawn wait sleep yield
    ping
    bg fg jobs kill ps top
    procstat sysstat
    diskinfo mkfsfat32 mount partctl
    shutdown restart exit clear ansi ansitest wavplay fastfetch memstat fsstat taskstat userstat
    shstat stats tty dmesg kbdstat mkdir touch write append cp mv rm kdbg bmpview qrcode
)

foreach(SRC IN LISTS USER_APP_MAIN_SOURCES)
    get_filename_component(_stem "${SRC}" NAME_WE)
    string(REGEX REPLACE "_main$" "" _app_name "${_stem}")
    string(TOUPPER "${_app_name}" _app_name_upper)
    string(REGEX REPLACE "[^A-Z0-9]" "_" _app_name_key "${_app_name_upper}")
    set(_app_enabled_var "CLEONOS_USER_APP_${_app_name_key}")

    list(FIND USER_KELF_APP_NAMES "${_app_name}" _shadowed_by_kmain)
    if(NOT _shadowed_by_kmain EQUAL -1)
        cl_log_info("skip ${SRC} because ${_app_name}_kmain.c exists")
        continue()
    endif()

    if(NOT DEFINED ${_app_enabled_var})
        set(${_app_enabled_var} ON CACHE BOOL "Build user app ${_app_name}")
    else()
        set(${_app_enabled_var} ${${_app_enabled_var}} CACHE BOOL "Build user app ${_app_name}" FORCE)
    endif()

    if(NOT ${_app_enabled_var})
        cl_log_info("menuconfig: skip disabled user app ${_app_name}")
        continue()
    endif()

    if(_app_name STREQUAL "doom" AND NOT DOOMGENERIC_AVAILABLE)
        cl_log_warn("skip user app doom because doomgeneric sources are unavailable")
        continue()
    endif()

    list(FIND USER_APP_NAMES "${_app_name}" _dup_name_idx)
    if(NOT _dup_name_idx EQUAL -1)
        cl_log_error("duplicate user app name: ${_app_name}")
    endif()
    list(APPEND USER_APP_NAMES "${_app_name}")

    add_user_c_object("${SRC}" _user_obj)

    set(_app_specific_objs)
    file(GLOB _app_specific_abs CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/cleonos/c/apps/${_app_name}_*.c"
        "${CMAKE_SOURCE_DIR}/cleonos/c/apps/${_app_name}/*.c"
    )
    set(_app_common_objects ${USER_COMMON_OBJECTS})

    if(_app_name STREQUAL "doom")
        foreach(_doom_src IN LISTS DOOMGENERIC_APP_SOURCES)
            list(APPEND _app_specific_abs "${CMAKE_SOURCE_DIR}/${_doom_src}")
        endforeach()
    endif()

    list(FIND USER_SHELL_COMMAND_APPS "${_app_name}" _shell_cmd_idx)
    if(NOT _shell_cmd_idx EQUAL -1)
        list(APPEND _app_specific_abs "${CMAKE_SOURCE_DIR}/cleonos/c/apps/cmd_runtime.c")
    endif()
    foreach(_extra_abs IN LISTS _app_specific_abs)
        file(RELATIVE_PATH _extra_rel "${CMAKE_SOURCE_DIR}" "${_extra_abs}")

        if(_extra_rel STREQUAL "${SRC}")
            continue()
        endif()

        if(_extra_rel MATCHES "_kmain\\.c$")
            continue()
        endif()

        if(_app_name STREQUAL "doom")
            add_user_c_object_doom("${_extra_rel}" _extra_obj)
        else()
            add_user_c_object("${_extra_rel}" _extra_obj)
        endif()
        list(APPEND _app_specific_objs "${_extra_obj}")
    endforeach()
    list(REMOVE_DUPLICATES _app_specific_objs)

    set(_app_out "${USER_APP_DIR}/${_app_name}.elf")
    add_custom_command(
        OUTPUT "${_app_out}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${USER_APP_DIR}"
        COMMAND ${USER_LD} ${USER_LDFLAGS} -o "${_app_out}" ${_app_common_objects} "${_user_obj}" ${_app_specific_objs} "${USER_RUST_LIB}"
        DEPENDS ${_app_common_objects} "${_user_obj}" ${_app_specific_objs} "${USER_RUST_LIB}" "${USER_LINKER_SCRIPT}"
        VERBATIM
    )

    list(APPEND USER_APP_OUTPUTS "${_app_out}")
    if(_app_name MATCHES ".*drv$")
        list(APPEND RAMDISK_DRIVER_APPS "${_app_out}")
    elseif(_app_name STREQUAL "hello")
        list(APPEND RAMDISK_ROOT_APPS "${_app_out}")
    else()
        list(APPEND RAMDISK_SHELL_APPS "${_app_out}")
    endif()
endforeach()

foreach(SRC IN LISTS USER_APP_KMAIN_SOURCES)
    get_filename_component(_stem "${SRC}" NAME_WE)
    string(REGEX REPLACE "_kmain$" "" _app_name "${_stem}")
    string(TOUPPER "${_app_name}" _app_name_upper)
    string(REGEX REPLACE "[^A-Z0-9]" "_" _app_name_key "${_app_name_upper}")
    set(_app_enabled_var "CLEONOS_USER_APP_${_app_name_key}")

    if(NOT DEFINED ${_app_enabled_var})
        set(${_app_enabled_var} ON CACHE BOOL "Build user app ${_app_name}")
    else()
        set(${_app_enabled_var} ${${_app_enabled_var}} CACHE BOOL "Build user app ${_app_name}" FORCE)
    endif()

    if(NOT ${_app_enabled_var})
        cl_log_info("menuconfig: skip disabled user app ${_app_name}")
        continue()
    endif()

    list(FIND USER_APP_NAMES "${_app_name}" _dup_name_idx)
    if(NOT _dup_name_idx EQUAL -1)
        cl_log_error("duplicate user app name: ${_app_name}")
    endif()
    list(APPEND USER_APP_NAMES "${_app_name}")

    add_user_c_object("${SRC}" _kelf_obj)
    set(_app_out "${USER_APP_DIR}/${_app_name}.elf")
    add_custom_command(
        OUTPUT "${_app_out}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${USER_APP_DIR}"
        COMMAND ${USER_LD} ${KELF_LDFLAGS} -o "${_app_out}" "${_kelf_obj}"
        DEPENDS "${_kelf_obj}" "${KELF_LINKER_SCRIPT}"
        VERBATIM
    )

    list(APPEND USER_APP_OUTPUTS "${_app_out}")
    list(APPEND RAMDISK_SYSTEM_APPS "${_app_out}")
endforeach()

if(NOT USER_APP_OUTPUTS)
    cl_log_error("no user ELF apps were discovered")
endif()

add_custom_target(userapps DEPENDS ${USER_APP_OUTPUTS})

add_custom_command(
    TARGET userapps
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        "-DNO_COLOR=${NO_COLOR}"
        "-DLOG_LEVEL=INFO"
        "-DLOG_TEXT=user elf apps ready"
        -P "${CL_LOG_EMIT_SCRIPT}"
)

file(GLOB_RECURSE RAMDISK_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/ramdisk/*")
set(RAMDISK_ROOT_STAMP "${BUILD_ROOT}/.ramdisk_root.stamp")
set(RAMDISK_COPY_COMMANDS)
foreach(_app IN LISTS RAMDISK_SHELL_APPS)
    get_filename_component(_app_file "${_app}" NAME)
    list(APPEND RAMDISK_COPY_COMMANDS COMMAND ${CMAKE_COMMAND} -E copy "${_app}" "${RAMDISK_ROOT}/shell/${_app_file}")
endforeach()
foreach(_app IN LISTS RAMDISK_ROOT_APPS)
    get_filename_component(_app_file "${_app}" NAME)
    list(APPEND RAMDISK_COPY_COMMANDS COMMAND ${CMAKE_COMMAND} -E copy "${_app}" "${RAMDISK_ROOT}/${_app_file}")
endforeach()
foreach(_app IN LISTS RAMDISK_DRIVER_APPS)
    get_filename_component(_app_file "${_app}" NAME)
    list(APPEND RAMDISK_COPY_COMMANDS COMMAND ${CMAKE_COMMAND} -E copy "${_app}" "${RAMDISK_ROOT}/driver/${_app_file}")
endforeach()
foreach(_app IN LISTS RAMDISK_SYSTEM_APPS)
    get_filename_component(_app_file "${_app}" NAME)
    list(APPEND RAMDISK_COPY_COMMANDS COMMAND ${CMAKE_COMMAND} -E copy "${_app}" "${RAMDISK_ROOT}/system/${_app_file}")
endforeach()

add_custom_command(
    OUTPUT "${RAMDISK_ROOT_STAMP}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${RAMDISK_ROOT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RAMDISK_ROOT}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/ramdisk" "${RAMDISK_ROOT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RAMDISK_ROOT}/system" "${RAMDISK_ROOT}/shell" "${RAMDISK_ROOT}/driver" "${RAMDISK_ROOT}/dev"
    COMMAND ${CMAKE_COMMAND} -E copy "${KERNEL_SYMBOLS_FILE}" "${RAMDISK_ROOT}/system/kernel.sym"
    ${RAMDISK_COPY_COMMANDS}
    COMMAND ${CMAKE_COMMAND} -E touch "${RAMDISK_ROOT_STAMP}"
    DEPENDS ${RAMDISK_FILES} ${USER_APP_OUTPUTS} kernel-symbols
    VERBATIM
)

add_custom_target(ramdisk-root DEPENDS "${RAMDISK_ROOT_STAMP}")
add_dependencies(ramdisk-root userapps)
add_dependencies(ramdisk-root kernel-symbols)

add_custom_command(
    OUTPUT "${RAMDISK_IMAGE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_ROOT}"
    COMMAND ${TAR} -C "${RAMDISK_ROOT}" -cf "${RAMDISK_IMAGE}" .
    DEPENDS "${RAMDISK_ROOT_STAMP}"
    VERBATIM
)

add_custom_target(ramdisk DEPENDS "${RAMDISK_IMAGE}")
