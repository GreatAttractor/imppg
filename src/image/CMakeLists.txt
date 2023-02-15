add_library(image STATIC
    src/image.cpp
    src/image.cpp
)

if(USE_FREEIMAGE EQUAL 0)
    target_sources(image PRIVATE
        src/bmp.cpp
        src/bmp.h
        src/tiff.cpp
        src/tiff.h
    )
endif()

set_compiler_options(image)

if(USE_FREEIMAGE EQUAL 1)
    target_compile_definitions(alignment PRIVATE USE_FREEIMAGE=1)
endif()

target_include_directories(image PUBLIC include)
target_include_directories(image PRIVATE ${Boost_INCLUDE_DIRS})

target_link_libraries(image PRIVATE common)

if(USE_CFITSIO EQUAL 1)
    target_include_directories(image PRIVATE ${CFITSIO_INCLUDE_DIRS})
    target_link_libraries(image PRIVATE ${CFITSIO_LIBRARIES})
endif()

if(USE_FREEIMAGE EQUAL 1)
    # Cannot do `pkg_check_modules` on `freeimage`; as of FreeImage 3.18.0, there is no `.pc` file provided (checked in MSYS2 and Fedora 29)
    target_link_libraries(image PRIVATE freeimage)
endif()
