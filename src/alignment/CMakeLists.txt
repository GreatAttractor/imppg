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

set_compiler_options(alignment)

if(USE_FREEIMAGE EQUAL 1)
    target_compile_definitions(alignment PRIVATE USE_FREEIMAGE=1)
endif()

target_include_directories(alignment PUBLIC include)
target_include_directories(alignment PRIVATE src ${Boost_INCLUDE_DIRS})

target_link_libraries(alignment PRIVATE ${wxWidgets_LIBRARIES} common image logging math_utils)
