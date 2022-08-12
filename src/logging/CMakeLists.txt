add_library(logging STATIC
    src/logging.cpp
)

target_include_directories(logging PUBLIC include)
