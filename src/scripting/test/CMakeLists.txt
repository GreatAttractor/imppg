add_executable(scripting_tests
    filesystem_tests.cpp
    main.cpp
    scripting_tests.cpp
    ScriptTestFixture.cpp
    ScriptTestFixture.h
)

target_link_libraries(scripting_tests PRIVATE scripting)

include(FindPkgConfig)
find_package(Boost REQUIRED
    unit_test_framework
)
target_include_directories(scripting_tests PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(scripting_tests PRIVATE ${Boost_LIBRARIES} ${wxWidgets_LIBRARIES})

add_test(NAME scripting COMMAND scripting_tests)
