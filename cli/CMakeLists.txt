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

install(TARGETS autoremesher-cli RUNTIME DESTINATION bin)
