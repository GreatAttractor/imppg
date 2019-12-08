#
# ImPPG build file
#
# Prerequisite libraries: wxWidgets 3.0, Boost 1.57.0, FreeImage (optional) 3.14.0 or newer, CFITSIO (optional).
# Before building, edit config.in to configure optional libs.
#
# NOTE: CMake relies on the presence of the "wx-config" tool to detect and configure wxWidgets-related build options.
#       Sometimes this tool can be named differently, e.g. in Fedora 23 with wxGTK3 packages from repository it is
#       "wx-config-3.0". This can be remedied e.g. by creating a symlink:
#
#         sudo ln -s /usr/bin/wx-config-3.0 /usr/bin/wx-config
#

project("ImPPG")

cmake_minimum_required(VERSION 2.8)

include(FindPkgConfig)
include(config.in)

add_executable(imppg WIN32
    src/about.cpp
    src/align_disc.cpp
    src/align_params.cpp
    src/align_phasecorr.cpp
    src/align_proc.cpp
    src/align_progress.cpp
    src/appconfig.cpp

    src/backend/cpu_bmp/cpu_bmp_core.cpp
    src/backend/cpu_bmp/cpu_bmp_proc.cpp
    src/backend/cpu_bmp/cpu_bmp_proc.h
    src/backend/cpu_bmp/w_lrdeconv.cpp
    src/backend/cpu_bmp/w_tcurve.cpp
    src/backend/cpu_bmp/w_unshmask.cpp
    src/backend/cpu_bmp/worker.cpp

    src/backend/opengl/gl_utils.cpp
    src/backend/opengl/gl_utils.h
    src/backend/opengl/opengl_display.cpp
    src/backend/opengl/opengl_display.h
    src/backend/opengl/opengl_proc.cpp
    src/backend/opengl/opengl_proc.h
    src/backend/opengl/uniforms.cpp
    src/backend/opengl/uniforms.h

    src/batch_params.cpp
    src/batch.cpp
    src/bmp.cpp
    src/common.cpp
    src/cursors.cpp
    src/fft.cpp
    src/formats.cpp
    src/gauss.cpp
    src/image.cpp
    src/logging.cpp
    src/lrdeconv.cpp
    src/main_window.cpp
    src/main.cpp
    src/normalize.cpp
    src/num_ctrl.cpp
    src/scrollable_dlg.cpp
    src/scrolled_view.cpp
    src/settings.cpp
    src/tcrv_edit.cpp
    src/tcrv_wnd_settings.cpp
    src/tcrv.cpp
    src/tiff.cpp
    src/wxapp.cpp
)
target_include_directories(imppg PRIVATE src)
target_compile_options(imppg PRIVATE -Werror -Wall -Wextra -Wpedantic -Wold-style-cast -Wno-parentheses -Wfatal-errors)

if (CMAKE_VERSION VERSION_LESS "3.1")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
    endif()
else()
    set_property(TARGET imppg PROPERTY CXX_STANDARD 17)
    set_property(TARGET imppg PROPERTY CXX_STANDARD_REQUIRED ON)
endif()

if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINK_FLAGS} -Wl,--strip-all")
endif()

if(WIN32)
    # The imppg(64).rc files include the manifest that enables MS Windows Common Controls 6.0.0.0
    # (for Windows XP and above), i.e. the nicer-looking, themed ones. It also defines the application icon.
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        # 32-bit build
        target_sources(imppg PUBLIC src/imppg.rc)
    else()
        # 64-bit build
        target_sources(imppg PUBLIC src/imppg64.rc)
    endif()
endif()

find_package(Boost REQUIRED)
target_include_directories(imppg PUBLIC ${Boost_INCLUDE_DIRS})

set(wxWidgets_USE_UNICODE TRUE)
set(wxWidgets_USE_DEBUG FALSE)
find_package(wxWidgets REQUIRED COMPONENTS adv aui xml core base gl) # TODO add `gl` conditionally
include(${wxWidgets_USE_FILE})
target_link_libraries(imppg ${wxWidgets_LIBRARIES})

find_package(OpenMP)
if(OPENMP_FOUND)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-ffast-math GNU_FFAST_MATH_SUPPORTED)

if(GNU_FFAST_MATH_SUPPORTED)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")
endif()

if(USE_FREEIMAGE EQUAL 1)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_FREEIMAGE")
    # Cannot do `pkg_check_modules` on `freeimage`; as of FreeImage 3.18.0, there is no `.pc` file provided (checked in MSYS2 and Fedora 29)
    target_link_libraries(imppg freeimage)
endif()

# TODO: conditionally
target_link_libraries(imppg GL)

# TODO: conditionally
find_package(GLEW REQUIRED)
target_include_directories(imppg PUBLIC ${GLEW_INCLUDE_DIRS})
target_link_libraries(imppg ${GLEW_LIBRARIES})

if(USE_CFITSIO EQUAL 1)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_CFITSIO")
    pkg_check_modules(CFITSIO cfitsio REQUIRED)
    target_include_directories(imppg PUBLIC ${CFITSIO_INCLUDE_DIRS})
    target_link_libraries(imppg ${CFITSIO_LIBRARIES})
endif()
