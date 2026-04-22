if(NOT DEFINED DISK_IMAGE OR "${DISK_IMAGE}" STREQUAL "")
    message(FATAL_ERROR "ensure_disk_image: DISK_IMAGE is required")
endif()

if(NOT DEFINED DISK_MB OR "${DISK_MB}" STREQUAL "")
    set(DISK_MB 64)
endif()

math(EXPR _disk_mb_int "${DISK_MB} + 0")
if(_disk_mb_int LESS 4)
    set(_disk_mb_int 4)
endif()

math(EXPR _disk_bytes "${_disk_mb_int} * 1024 * 1024")
get_filename_component(_disk_dir "${DISK_IMAGE}" DIRECTORY)
if(NOT EXISTS "${_disk_dir}")
    file(MAKE_DIRECTORY "${_disk_dir}")
endif()

set(_need_create TRUE)
if(EXISTS "${DISK_IMAGE}")
    file(SIZE "${DISK_IMAGE}" _disk_size_now)
    if(_disk_size_now GREATER_EQUAL _disk_bytes)
        set(_need_create FALSE)
    endif()
endif()

if(_need_create)
    find_program(_python_exec NAMES python3 python py)
    if(_python_exec)
        execute_process(
            COMMAND "${_python_exec}" "-c" "import os,sys; p=sys.argv[1]; sz=int(sys.argv[2]); d=os.path.dirname(p); d and os.makedirs(d, exist_ok=True); f=open(p,'ab'); f.truncate(sz); f.close()"
                "${DISK_IMAGE}" "${_disk_bytes}"
            RESULT_VARIABLE _python_result
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if(_python_result EQUAL 0)
            set(_need_create FALSE)
        endif()
    endif()
endif()

if(_need_create)
    find_program(_dd_exec NAMES dd)
    if(_dd_exec)
        execute_process(
            COMMAND "${_dd_exec}" "if=/dev/zero" "of=${DISK_IMAGE}" "bs=1M" "count=${_disk_mb_int}"
            RESULT_VARIABLE _dd_result
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if(_dd_result EQUAL 0)
            set(_need_create FALSE)
        endif()
    endif()
endif()

if(_need_create)
    message(FATAL_ERROR "ensure_disk_image: failed to create disk image '${DISK_IMAGE}' (${_disk_mb_int}MB)")
endif()

if(NOT EXISTS "${DISK_IMAGE}")
    message(FATAL_ERROR "ensure_disk_image: disk image missing after create '${DISK_IMAGE}'")
endif()

file(SIZE "${DISK_IMAGE}" _disk_size_final)
if(_disk_size_final LESS _disk_bytes)
    message(FATAL_ERROR "ensure_disk_image: disk image too small (${_disk_size_final} bytes), expected >= ${_disk_bytes}")
endif()

message(STATUS "ensure_disk_image: ready '${DISK_IMAGE}' (${_disk_size_final} bytes)")
