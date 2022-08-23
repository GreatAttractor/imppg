add_library(logging STATIC
    src/logging.cpp
)

include(../../utils.cmake)
set_cpp_standard(logging)

target_include_directories(logging PUBLIC include)
