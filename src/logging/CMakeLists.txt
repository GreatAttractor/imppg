add_library(logging STATIC
    src/logging.cpp
)

set_cpp_standard(logging)

target_include_directories(logging PUBLIC include)
