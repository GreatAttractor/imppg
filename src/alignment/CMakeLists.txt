add_library(alignment STATIC
    src/align_common.h
    src/align_disc.cpp
    src/align_disc.h
    src/align_phasecorr.cpp
    src/align_phasecorr.h
    src/align_proc.cpp
    src/fft.cpp
    src/fft.h
)

include(../../utils.cmake)
set_compiler_options(alignment)

target_include_directories(alignment PUBLIC include)
target_include_directories(alignment PRIVATE src ${Boost_INCLUDE_DIRS})

target_link_libraries(alignment PRIVATE ${wxWidgets_LIBRARIES} common logging) # image  math_utils)
