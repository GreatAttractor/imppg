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

# Workaround: Apple Clang 21 (Xcode on macOS Tahoe) frontend segfaults compiling
# align_phasecorr.cpp at -O2/-O3 with Boost 1.90 uBLAS templates. Compile this
# file at -O1 to avoid the crash.
if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set_source_files_properties(src/align_phasecorr.cpp PROPERTIES COMPILE_FLAGS "-O1")
endif()

if(USE_FREEIMAGE EQUAL 1)
    target_compile_definitions(alignment PRIVATE USE_FREEIMAGE=1)
endif()

target_include_directories(alignment PUBLIC include)
target_include_directories(alignment PRIVATE src ${Boost_INCLUDE_DIRS})

target_link_libraries(alignment PRIVATE ${wxWidgets_LIBRARIES} common image logging math_utils)
