add_library(common STATIC
    src/common.cpp
    src/formats.cpp
    src/scrolled_view.cpp
    src/tcrv.cpp
)

target_compile_definitions(common PRIVATE
    IMPPG_IMAGES_DIR="${IMPPG_IMAGES_DIR}"
)

target_include_directories(common PUBLIC include)

target_link_libraries(common PRIVATE image math_utils)
