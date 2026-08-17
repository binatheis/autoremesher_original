# Multi-interpreter Python module targets.
#
# FindPython caches its results on first use, so it cannot describe two
# different interpreters in a single CMake configure. Instead we query each
# interpreter's sysconfig directly and build module targets manually:
#
#   ar_add_python_module(cp311 "C:/Python311/python.exe" FALSE)
#   ar_add_python_module(abi3  "C:/Python312/python.exe" TRUE)
#
# The first produces autoremesher_engine.<EXT_SUFFIX>; the second produces a
# stable-ABI module (Py_LIMITED_API=0x030C0000, .abi3 suffix) that imports on
# every CPython >= 3.12. nanobind is compiled from the Conan package sources
# (nb_combined.cpp) once per target because its ABI depends on the Python
# version it is compiled against.

function(ar_query_python OUT_VAR PYTHON_EXE)
    execute_process(
        COMMAND "${PYTHON_EXE}" -c "import sys, sysconfig
print(sysconfig.get_paths()['include'])
print(sysconfig.get_config_var('EXT_SUFFIX') or '.so')
print(sysconfig.get_config_var('installed_base') or sys.base_prefix)
print(f'{sys.version_info.major}{sys.version_info.minor}')"
        OUTPUT_VARIABLE _out
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_VARIABLE _err
        RESULT_VARIABLE _res)
    if(NOT _res EQUAL 0)
        message(FATAL_ERROR "Failed to query ${PYTHON_EXE}: ${_err}")
    endif()
    string(REPLACE "\r" "" _out "${_out}")
    string(REPLACE "\n" ";" _lines "${_out}")
    list(GET _lines 0 _include)
    list(GET _lines 1 _ext_suffix)
    list(GET _lines 2 _base)
    list(GET _lines 3 _vernum)
    file(TO_CMAKE_PATH "${_include}" _include)
    file(TO_CMAKE_PATH "${_base}" _base)
    set(${OUT_VAR}_INCLUDE "${_include}" PARENT_SCOPE)
    set(${OUT_VAR}_EXT_SUFFIX "${_ext_suffix}" PARENT_SCOPE)
    set(${OUT_VAR}_BASE "${_base}" PARENT_SCOPE)
    set(${OUT_VAR}_VERNUM "${_vernum}" PARENT_SCOPE)
endfunction()

function(ar_add_python_module TAG PYTHON_EXE ABI3)
    if(NOT EXISTS "${PYTHON_EXE}")
        message(FATAL_ERROR "Python interpreter not found: ${PYTHON_EXE}")
    endif()
    if(NOT DEFINED NANOBIND_ROOT OR NOT EXISTS "${NANOBIND_ROOT}/nanobind/src/nb_combined.cpp")
        message(FATAL_ERROR "NANOBIND_ROOT does not point at a Conan nanobind package: ${NANOBIND_ROOT}")
    endif()

    ar_query_python(PY_${TAG} "${PYTHON_EXE}")
    set(_include "${PY_${TAG}_INCLUDE}")
    set(_suffix "${PY_${TAG}_EXT_SUFFIX}")
    set(_base "${PY_${TAG}_BASE}")
    set(_vernum "${PY_${TAG}_VERNUM}")
    if(ABI3)
        if(WIN32)
            # Windows wheels ship abi3 modules as plain ".pyd": the abi3 tag
            # lives in the wheel filename only, and CPython's Windows import
            # system does not list ".abi3.pyd" in EXTENSION_SUFFIXES.
            set(_suffix ".pyd")
        else()
            set(_suffix ".abi3.so")
        endif()
    endif()

    # --- nanobind core, compiled against this interpreter -------------------
    add_library(nanobind_${TAG} OBJECT "${NANOBIND_ROOT}/nanobind/src/nb_combined.cpp")
    target_include_directories(nanobind_${TAG} PRIVATE
        "${_include}" "${NANOBIND_ROOT}/nanobind/include")
    target_link_libraries(nanobind_${TAG} PRIVATE tsl::robin_map)
    target_compile_features(nanobind_${TAG} PUBLIC cxx_std_17)
    set_target_properties(nanobind_${TAG} PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
    target_compile_definitions(nanobind_${TAG} PRIVATE
        $<$<CONFIG:Release,MinSizeRel,RelWithDebInfo>:NB_COMPACT_ASSERTIONS>)
    if(ABI3)
        target_compile_definitions(nanobind_${TAG} PUBLIC Py_LIMITED_API=0x030C0000)
    endif()
    if(MSVC)
        target_compile_definitions(nanobind_${TAG} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(nanobind_${TAG} PRIVATE
            -fno-strict-aliasing -ffunction-sections -fdata-sections)
    endif()

    # --- extension module ----------------------------------------------------
    add_library(autoremesher_engine_${TAG} MODULE
        "${CMAKE_CURRENT_SOURCE_DIR}/bindings.cpp"
        $<TARGET_OBJECTS:nanobind_${TAG}>)
    target_include_directories(autoremesher_engine_${TAG} PRIVATE
        "${_include}" "${NANOBIND_ROOT}/nanobind/include")
    target_link_libraries(autoremesher_engine_${TAG} PRIVATE
        autoremesher_core tsl::robin_map)
    if(ABI3)
        target_compile_definitions(autoremesher_engine_${TAG} PRIVATE Py_LIMITED_API=0x030C0000)
    endif()
    set(_out_dir "${CMAKE_BINARY_DIR}/python/${TAG}")
    set_target_properties(autoremesher_engine_${TAG} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "autremesh"
        ARCHIVE_OUTPUT_NAME "autoremesher_engine_${TAG}"
        SUFFIX "${_suffix}"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        LIBRARY_OUTPUT_DIRECTORY "${_out_dir}"
        RUNTIME_OUTPUT_DIRECTORY "${_out_dir}"
        ARCHIVE_OUTPUT_DIRECTORY "${_out_dir}"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${_out_dir}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${_out_dir}"
        ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${_out_dir}")

    # Python import library (Windows only; other platforms resolve at import)
    if(WIN32)
        if(ABI3)
            set(_pylib "${_base}/libs/python3.lib")
        else()
            set(_pylib "${_base}/libs/python${_vernum}.lib")
        endif()
        if(NOT EXISTS "${_pylib}")
            message(FATAL_ERROR "Expected Python library not found: ${_pylib}")
        endif()
        target_link_libraries(autoremesher_engine_${TAG} PRIVATE "${_pylib}")
    elseif(APPLE)
        target_link_options(autoremesher_engine_${TAG} PRIVATE -undefined dynamic_lookup)
    endif()

    if(NOT MSVC)
        target_link_options(autoremesher_engine_${TAG} PRIVATE
            $<$<PLATFORM_ID:Linux>:-Wl,--gc-sections>
            # On Linux, static libraries linked into a shared module export
            # their symbols with default visibility, even when the module
            # itself uses CXX_VISIBILITY_PRESET hidden. TBB's exported
            # symbols (tbb::internal::*, governor, task_scheduler, etc.)
            # clash with symbols from other loaded libraries (numpy, system
            # TBB, etc.) and cause a segfault at import time. --exclude-libs
            # prevents any static-library symbol from being exported, so
            # only the nanobind PyInit_ entry point remains visible.
            $<$<PLATFORM_ID:Linux>:-Wl,--exclude-libs,ALL>)
    endif()

    message(STATUS "Python module [${TAG}]: ${PYTHON_EXE}")
    message(STATUS "    include: ${_include}")
    message(STATUS "    suffix : ${_suffix}")
endfunction()
