add_library(math_utils STATIC
    src/convolution.cpp
    src/gauss.cpp
)

set_cpp_standard(math_utils)

target_include_directories(math_utils PUBLIC include)
