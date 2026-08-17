# Python bindings (nanobind). One module per requested interpreter.
#
# - AR_PYTHON311_EXECUTABLE → autoremesher_engine.<cp311 suffix>  (dedicated)
# - AR_PYTHON312_EXECUTABLE → autoremesher_engine.abi3.{so,pyd}   (3.12+ stable ABI)
#
# tsl-robin-map is nanobind's only dependency; Conan (CMakeDeps) provides it.

find_package(tsl-robin-map CONFIG REQUIRED)

include(${CMAKE_SOURCE_DIR}/cmake/PythonTargets.cmake)

if(DEFINED AR_PYTHON311_EXECUTABLE)
    ar_add_python_module(cp311 "${AR_PYTHON311_EXECUTABLE}" FALSE)
endif()

if(DEFINED AR_PYTHON312_EXECUTABLE)
    ar_add_python_module(abi3 "${AR_PYTHON312_EXECUTABLE}" TRUE)
endif()

if(NOT DEFINED AR_PYTHON311_EXECUTABLE AND NOT DEFINED AR_PYTHON312_EXECUTABLE)
    message(WARNING "AR_BUILD_BINDINGS is ON but no interpreter was provided; "
                    "set AR_PYTHON311_EXECUTABLE and/or AR_PYTHON312_EXECUTABLE.")
endif()
