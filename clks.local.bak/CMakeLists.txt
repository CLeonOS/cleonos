set(KERNEL_SOURCE_DIRS
    "${CMAKE_SOURCE_DIR}/clks/kernel"
    "${CMAKE_SOURCE_DIR}/clks/arch/${CLKS_ARCH}"
    "${CMAKE_SOURCE_DIR}/clks/third_party"
)

set(C_SOURCES)
set(CPP_SOURCES)
set(ASM_SOURCES)
foreach(_src_dir IN LISTS KERNEL_SOURCE_DIRS)
    if(EXISTS "${_src_dir}")
        file(GLOB_RECURSE _c_abs CONFIGURE_DEPENDS "${_src_dir}/*.c")
        file(GLOB_RECURSE _cpp_abs CONFIGURE_DEPENDS "${_src_dir}/*.cpp")
        file(GLOB_RECURSE _s_abs CONFIGURE_DEPENDS "${_src_dir}/*.S")
        foreach(_abs IN LISTS _c_abs)
            file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}" "${_abs}")
            list(APPEND C_SOURCES "${_rel}")
        endforeach()
        foreach(_abs IN LISTS _cpp_abs)
            file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}" "${_abs}")
            list(APPEND CPP_SOURCES "${_rel}")
        endforeach()
        foreach(_abs IN LISTS _s_abs)
            file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}" "${_abs}")
            list(APPEND ASM_SOURCES "${_rel}")
        endforeach()
    endif()
endforeach()
list(REMOVE_DUPLICATES C_SOURCES)
list(REMOVE_DUPLICATES CPP_SOURCES)
list(REMOVE_DUPLICATES ASM_SOURCES)
list(SORT C_SOURCES)
list(SORT CPP_SOURCES)
list(SORT ASM_SOURCES)

set(CLKS_BOOT_LIMINE_SOURCE "clks/kernel/boot/limine/limine_requests.c")
if(EXISTS "${CMAKE_SOURCE_DIR}/${CLKS_BOOT_LIMINE_SOURCE}")
    list(APPEND C_SOURCES "${CLKS_BOOT_LIMINE_SOURCE}")
    list(REMOVE_DUPLICATES C_SOURCES)
    list(SORT C_SOURCES)
else()
    cl_log_error("missing required boot source: ${CLKS_BOOT_LIMINE_SOURCE}")
endif()

file(GLOB_RECURSE KERNEL_INC_SOURCES_ABS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/clks/**/*.inc"
)
list(SORT KERNEL_INC_SOURCES_ABS)

if(NOT C_SOURCES)
    cl_log_error("no kernel C sources found in clks/")
endif()
if(NOT EXISTS "${LINKER_SCRIPT}")
    cl_log_error("missing linker script: ${LINKER_SCRIPT}")
endif()

function(add_kernel_c_object SRC OUTPUT_LIST_VAR)
    string(REGEX REPLACE "\\.c$" ".o" OBJ_REL "${SRC}")
    set(OBJ_PATH "${OBJ_ROOT}/${OBJ_REL}")
    get_filename_component(OBJ_DIR "${OBJ_PATH}" DIRECTORY)
    set(_deps "${CMAKE_SOURCE_DIR}/${SRC}" ${KERNEL_INC_SOURCES_ABS})
    set(_cflags ${CFLAGS_COMMON})

    if("${SRC}" MATCHES "^clks/third_party/")
        list(APPEND _cflags
            -Wno-error
            -Wno-unused-function
            -Wno-unused-parameter
            -Wno-sign-compare
            -Wno-type-limits
            -Wno-missing-field-initializers
        )
    endif()

    add_custom_command(
        OUTPUT "${OBJ_PATH}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OBJ_DIR}"
        COMMAND ${CC} ${_cflags} ${ARCH_CFLAGS} -c "${CMAKE_SOURCE_DIR}/${SRC}" -o "${OBJ_PATH}"
        DEPENDS ${_deps}
        VERBATIM
    )

    set(${OUTPUT_LIST_VAR} ${${OUTPUT_LIST_VAR}} "${OBJ_PATH}" PARENT_SCOPE)
endfunction()

function(add_kernel_asm_object SRC OUTPUT_LIST_VAR)
    string(REGEX REPLACE "\\.S$" ".o" OBJ_REL "${SRC}")
    set(OBJ_PATH "${OBJ_ROOT}/${OBJ_REL}")
    get_filename_component(OBJ_DIR "${OBJ_PATH}" DIRECTORY)
    set(_deps "${CMAKE_SOURCE_DIR}/${SRC}" ${KERNEL_INC_SOURCES_ABS})

    add_custom_command(
        OUTPUT "${OBJ_PATH}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OBJ_DIR}"
        COMMAND ${CC} ${ASFLAGS_COMMON} ${ARCH_CFLAGS} -c "${CMAKE_SOURCE_DIR}/${SRC}" -o "${OBJ_PATH}"
        DEPENDS ${_deps}
        VERBATIM
    )

    set(${OUTPUT_LIST_VAR} ${${OUTPUT_LIST_VAR}} "${OBJ_PATH}" PARENT_SCOPE)
endfunction()

function(add_kernel_cpp_object SRC OUTPUT_LIST_VAR)
    string(REGEX REPLACE "\\.cpp$" ".o" OBJ_REL "${SRC}")
    set(OBJ_PATH "${OBJ_ROOT}/${OBJ_REL}")
    get_filename_component(OBJ_DIR "${OBJ_PATH}" DIRECTORY)
    set(_deps "${CMAKE_SOURCE_DIR}/${SRC}" ${KERNEL_INC_SOURCES_ABS})

    add_custom_command(
        OUTPUT "${OBJ_PATH}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OBJ_DIR}"
        COMMAND ${KERNEL_CXX} ${CXXFLAGS_COMMON} ${ARCH_CFLAGS} -c "${CMAKE_SOURCE_DIR}/${SRC}" -o "${OBJ_PATH}"
        DEPENDS ${_deps}
        VERBATIM
    )

    set(${OUTPUT_LIST_VAR} ${${OUTPUT_LIST_VAR}} "${OBJ_PATH}" PARENT_SCOPE)
endfunction()

set(KERNEL_OBJECTS)
foreach(SRC IN LISTS C_SOURCES)
    add_kernel_c_object("${SRC}" KERNEL_OBJECTS)
endforeach()
foreach(SRC IN LISTS CPP_SOURCES)
    add_kernel_cpp_object("${SRC}" KERNEL_OBJECTS)
endforeach()
foreach(SRC IN LISTS ASM_SOURCES)
    add_kernel_asm_object("${SRC}" KERNEL_OBJECTS)
endforeach()

add_custom_command(
    OUTPUT "${KERNEL_RUST_LIB}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_ROOT}"
    COMMAND ${RUSTC} --crate-type staticlib -C panic=abort -O "${CMAKE_SOURCE_DIR}/clks/rust/src/lib.rs" -o "${KERNEL_RUST_LIB}"
    DEPENDS "${CMAKE_SOURCE_DIR}/clks/rust/src/lib.rs"
    VERBATIM
)

add_custom_command(
    OUTPUT "${KERNEL_ELF}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_ROOT}"
    COMMAND ${LD} ${LDFLAGS_COMMON} -T "${LINKER_SCRIPT}" -o "${KERNEL_ELF}" ${KERNEL_OBJECTS} "${KERNEL_RUST_LIB}"
    DEPENDS ${KERNEL_OBJECTS} "${LINKER_SCRIPT}" "${KERNEL_RUST_LIB}" "${CMAKE_SOURCE_DIR}/clks/rust/src/lib.rs"
    VERBATIM
)

add_custom_target(kernel DEPENDS "${KERNEL_ELF}")

add_custom_command(
    OUTPUT "${KERNEL_SYMBOLS_FILE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_ROOT}"
    COMMAND ${CMAKE_COMMAND}
        "-DNM_TOOL=${NM}"
        "-DADDR2LINE_TOOL=${ADDR2LINE}"
        "-DKERNEL_ELF=${KERNEL_ELF}"
        "-DOUT_SYMBOL_FILE=${KERNEL_SYMBOLS_FILE}"
        -P "${CMAKE_SOURCE_DIR}/cmake/gen_kernel_symbols.cmake"
    DEPENDS "${KERNEL_ELF}" "${CMAKE_SOURCE_DIR}/cmake/gen_kernel_symbols.cmake"
    VERBATIM
)

add_custom_target(kernel-symbols DEPENDS "${KERNEL_SYMBOLS_FILE}")
add_dependencies(kernel-symbols kernel)
