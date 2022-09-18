add_library(common STATIC
    src/common.cpp
    src/formats.cpp
    src/num_formatter.cpp
    src/proc_settings.cpp
    src/scrolled_view.cpp
    src/tcrv.cpp
)

include(../../utils.cmake)
set_compiler_options(common)

target_compile_definitions(common PRIVATE
    IMPPG_IMAGES_DIR="${IMPPG_IMAGES_DIR}"
)

target_include_directories(common PUBLIC include)

target_link_libraries(common PRIVATE image math_utils ${wxWidgets_LIBRARIES})

if(USE_FREEIMAGE EQUAL 1)
    target_compile_definitions(common PRIVATE USE_FREEIMAGE=1)

    # Cannot do `pkg_check_modules` on `freeimage`; as of FreeImage 3.18.0, there is no `.pc` file provided (checked in MSYS2 and Fedora 29)
    target_link_libraries(common PRIVATE freeimage)
endif()
