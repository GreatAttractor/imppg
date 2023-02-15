add_executable(scripting_tests
    filesystem_tests.cpp
    main.cpp
    scripting_tests.cpp
    ScriptTestFixture.cpp
    ScriptTestFixture.h
    settings_tests.cpp
)

set_compiler_options(scripting_tests)

include(FindPkgConfig)
find_package(Boost REQUIRED
    unit_test_framework
)
target_include_directories(scripting_tests PRIVATE ${Boost_INCLUDE_DIRS})

target_link_libraries(scripting_tests PRIVATE
    ${Boost_LIBRARIES}
    ${wxWidgets_LIBRARIES}
    scripting
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(scripting_tests PRIVATE stdc++fs)
endif()

add_test(NAME scripting COMMAND scripting_tests)
