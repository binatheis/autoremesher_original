# GoogleTest suite for the core library.
find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

add_executable(autoremesher_tests
    main.cpp
    test_vector3.cpp
    test_remesh.cpp
    test_progress.cpp
)

target_link_libraries(autoremesher_tests PRIVATE
    autoremesher_core
    GTest::gtest
)

if(MSVC)
    target_compile_options(autoremesher_tests PRIVATE /bigobj)
endif()

gtest_discover_tests(autoremesher_tests
    PROPERTIES TIMEOUT 300
)
