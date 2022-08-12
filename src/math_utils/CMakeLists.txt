add_library(math_utils STATIC
    src/convolution.cpp
    src/gauss.cpp
)

target_include_directories(math_utils PUBLIC include)
