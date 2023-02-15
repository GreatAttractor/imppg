add_executable(common_tests
    processing_settings_tests.cpp
    main.cpp
)

set_compiler_options(common_tests)

include(FindPkgConfig)
find_package(Boost REQUIRED
    unit_test_framework
)
target_include_directories(common_tests PRIVATE ${Boost_INCLUDE_DIRS})

target_link_libraries(common_tests PRIVATE
    ${Boost_LIBRARIES}
    ${wxWidgets_LIBRARIES}
    common
)

add_test(NAME common COMMAND common_tests)
