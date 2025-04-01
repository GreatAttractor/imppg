add_library(backend STATIC
    src/cpu_bmp/cpu_bmp_core.cpp
    src/cpu_bmp/cpu_bmp_proc.cpp
    src/cpu_bmp/cpu_bmp_proc.h
    src/cpu_bmp/lrdeconv.cpp
    src/cpu_bmp/lrdeconv.h
    src/cpu_bmp/w_lrdeconv.cpp
    src/cpu_bmp/w_tcurve.cpp
    src/cpu_bmp/w_unshmask.cpp
    src/cpu_bmp/worker.cpp
    src/cpu_bmp/message_ids.h
)

if(USE_OPENGL_BACKEND)
    target_sources(backend PRIVATE
        src/opengl/gl_utils.cpp
        src/opengl/gl_utils.h
        src/opengl/opengl_display.cpp
        src/opengl/opengl_display.h
        src/opengl/opengl_proc.cpp
        src/opengl/opengl_proc.h
        src/opengl/uniforms.cpp
        src/opengl/uniforms.h
    )
endif()

set_compiler_options(backend)

target_include_directories(backend PUBLIC include)
target_include_directories(backend PRIVATE src ${Boost_INCLUDE_DIRS})

if(USE_OPENGL_BACKEND)
    if(WIN32)
        target_link_libraries(backend PRIVATE opengl32)
    else()
        target_link_libraries(backend PRIVATE GL)
    endif()

    find_package(GLEW REQUIRED)
    target_include_directories(backend PRIVATE ${GLEW_INCLUDE_DIRS})
    target_link_libraries(backend PRIVATE GLEW::GLEW)

    target_compile_definitions(backend PRIVATE IMPPG_SHADERS_DIR="${IMPPG_SHADERS_DIR}")
endif()

target_link_libraries(backend PUBLIC image PRIVATE ${wxWidgets_LIBRARIES} common logging math_utils)
