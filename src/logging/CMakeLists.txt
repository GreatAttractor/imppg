add_library(logging STATIC
    src/logging.cpp
)

include(../../utils.cmake)
set_compiler_options(logging)

target_include_directories(logging PUBLIC include)
