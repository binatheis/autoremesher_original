# Standalone Qt-free CLI built on the core library.
find_package(CLI11 CONFIG REQUIRED)

add_executable(autoremesher-cli
    main.cpp
    obj_io.cpp
)

target_link_libraries(autoremesher-cli PRIVATE
    autoremesher_core
    CLI11::CLI11
)

if(MSVC)
    target_compile_options(autoremesher-cli PRIVATE /W3 /bigobj)
endif()

set_target_properties(autoremesher-cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/cli")

# On Linux, oneTBB is a shared library from Conan. Set the install RPATH to
# $ORIGIN so the installed CLI finds libtbb.so in the same directory.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties(autoremesher-cli PROPERTIES
        INSTALL_RPATH "$ORIGIN")
endif()

install(TARGETS autoremesher-cli RUNTIME DESTINATION bin)

# Bundle the oneTBB shared library next to the CLI on Linux so the artifact
# is self-contained.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    install(FILES $<TARGET_FILE:TBB::tbb> DESTINATION bin)
endif()
