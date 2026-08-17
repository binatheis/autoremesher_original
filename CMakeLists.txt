# AutoRemesher library / bindings / CLI / tests build.
#
# This CMake build coexists with the upstream qmake project (autoremesher.pro):
# the .pro file keeps building the Qt GUI application, while this file builds
# the Qt-free core library, the Python bindings (nanobind), the standalone CLI
# (CLI11) and the test suite (GoogleTest).
#
# Dependency policy:
#   - Conan provides ONLY nanobind, CLI11 and GoogleTest (see conanfile.py).
#   - Eigen, TBB, meshoptimizer and isotropicremesher stay bundled under
#     thirdparty/, exactly as upstream v1.1.0 carries them.
cmake_minimum_required(VERSION 3.24)

# thirdparty/tbb is the legacy TBB whose cmake_minimum_required is 2.8, which
# CMake >= 4 rejects; this escape hatch keeps it configurable.
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

project(AutoRemesherBindings VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AR_BUILD_BINDINGS "Build Python bindings (nanobind)" ON)
option(AR_BUILD_CLI "Build the standalone CLI (CLI11)" ON)
option(AR_BUILD_TESTS "Build the GoogleTest suite" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# GCC 14 introduced -Wchanges-meaning and treats the bundled legacy TBB's
# task.h name reuse (a method named `task()` shadowing the class `task`) as
# a hard error. TBB headers are PUBLIC includes of autoremesher_core, so
# every target in the project (core, CLI, bindings, tests) is affected.
# Suppress the warning globally rather than per-target.
# ---------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 14)
    add_compile_options(-Wno-changes-meaning)
endif()

# ---------------------------------------------------------------------------
# Bundled TBB (static on every platform so the wheels/CLI are self-contained)
# ---------------------------------------------------------------------------
set(TBB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(TBB_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(TBB_BUILD_TBBMALLOC OFF CACHE BOOL "" FORCE)
set(TBB_BUILD_TBBMALLOC_PROXY OFF CACHE BOOL "" FORCE)
set(TBB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/tbb EXCLUDE_FROM_ALL)

# The bundled TBB predates C++17: with /std:c++17 the Windows SDK headers hit
# an ambiguity between std::byte and TBB's own unqualified `byte` uses
# (C2872). Keep TBB at C++14 while the rest of the project uses C++17.
if(TARGET tbb_static)
    set_property(TARGET tbb_static PROPERTY CXX_STANDARD 14)
    set_property(TARGET tbb_static PROPERTY CXX_STANDARD_REQUIRED ON)
endif()

# ---------------------------------------------------------------------------
# Core library: src/AutoRemesher + bundled isotropicremesher + meshoptimizer
# ---------------------------------------------------------------------------
file(GLOB AR_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AutoRemesher/*.cpp")

add_library(autoremesher_core STATIC
    ${AR_CORE_SOURCES}
    thirdparty/isotropicremesher/isotropicremesher.cpp
    thirdparty/isotropicremesher/isotropichalfedgemesh.cpp
    thirdparty/isotropicremesher/axisalignedboundingboxtree.cpp
    thirdparty/meshoptimizer/src/simplifier.cpp
    thirdparty/meshoptimizer/src/indexgenerator.cpp
)

target_include_directories(autoremesher_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/eigen
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/isotropicremesher
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/meshoptimizer/src
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/tbb/include
)

target_compile_definitions(autoremesher_core PUBLIC
    _USE_MATH_DEFINES
    NOMINMAX
)

# Static TBB: consumers must not rely on TBB's pragma-based auto-linking.
target_compile_definitions(autoremesher_core PUBLIC __TBB_NO_IMPLICIT_LINKAGE=1)
target_link_libraries(autoremesher_core PUBLIC tbb_static)

set_target_properties(autoremesher_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(autoremesher_core PRIVATE /bigobj /fp:fast)
    target_compile_definitions(autoremesher_core PRIVATE
        _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN)
elseif(APPLE)
    # Eigen::AccelerateSupport path in constrainedleastsquares.cpp
    target_compile_definitions(autoremesher_core PRIVATE AUTO_REMESHER_USE_ACCELERATE=1)
    target_compile_options(autoremesher_core PRIVATE -O3 -funroll-loops)
    target_link_libraries(autoremesher_core PUBLIC "-framework Accelerate")
else()
    # x86-64-v2 baseline: runs on Linux CPUs without AVX2 (as upstream .pro)
    target_compile_options(autoremesher_core PRIVATE -O3 -funroll-loops -march=x86-64-v2)
    target_link_libraries(autoremesher_core PUBLIC dl pthread)
endif()

if(AR_BUILD_CLI)
    add_subdirectory(cli)
endif()

if(AR_BUILD_BINDINGS)
    add_subdirectory(bindings/python)
endif()

if(AR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Machine-wide vcpkg MSBuild integration must not inject its own libraries
# (tbb, gmock, etc.) into our Conan/bundled build on Windows.
if(MSVC)
    function(ar_disable_vcpkg_for_all _dir)
        get_property(_targets DIRECTORY "${_dir}" PROPERTY BUILDSYSTEM_TARGETS)
        foreach(_t ${_targets})
            if(TARGET "${_t}")
                set_target_properties("${_t}" PROPERTIES
                    VS_GLOBAL_VcpkgEnabled "false"
                    VS_GLOBAL_VcpkgAutoLink "false")
            endif()
        endforeach()
        get_property(_subdirs DIRECTORY "${_dir}" PROPERTY SUBDIRECTORIES)
        foreach(_s ${_subdirs})
            ar_disable_vcpkg_for_all("${_s}")
        endforeach()
    endfunction()
    ar_disable_vcpkg_for_all("${CMAKE_SOURCE_DIR}")
endif()

message(STATUS "==================== AutoRemesher build ====================")
message(STATUS "  Build type      : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Python bindings : ${AR_BUILD_BINDINGS}")
message(STATUS "  CLI             : ${AR_BUILD_CLI}")
message(STATUS "  Tests           : ${AR_BUILD_TESTS}")
message(STATUS "  NANOBIND_ROOT   : ${NANOBIND_ROOT}")
message(STATUS "============================================================")
