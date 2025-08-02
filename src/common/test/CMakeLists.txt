add_executable(common_tests
    main.cpp
    processing_settings_tests.cpp
    tone_curve_tests.cpp
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

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(common_tests PRIVATE stdc++fs)
endif()


add_test(NAME common COMMAND common_tests)
