#
# ImPPG build file
#
# Prerequisite libraries: wxWidgets 3.0 (3.1 under MS Windows), Boost 1.57.0, FreeImage (optional) 3.14.0 or newer, CFITSIO (optional).
# Before building, edit config.cmake to configure optional libs.
#
# NOTE: CMake relies on the presence of the "wx-config" tool to detect and configure wxWidgets-related build options.
#       Sometimes this tool can be named differently, e.g. in Fedora 23 with wxGTK3 packages from repository it is
#       "wx-config-3.0". This can be remedied e.g. by creating a symlink:
#
#         sudo ln -s /usr/bin/wx-config-3.0 /usr/bin/wx-config
#

cmake_minimum_required(VERSION 2.8...3.5)

project("imppg")

enable_testing()

include(config.cmake)
include(utils.cmake)
include(FindPkgConfig)
include(GNUInstallDirs)

add_executable(imppg WIN32
    src/about.cpp
    src/adv_settings_wnd.cpp
    src/align_params.cpp
    src/align_progress.cpp
    src/appconfig.cpp
    src/batch_params.cpp
    src/batch.cpp
    src/cursors.cpp
    src/main_window.cpp
    src/main.cpp
    src/normalize.cpp
    src/num_ctrl.cpp
    src/progress_bar.cpp
    src/scrollable_dlg.cpp
    src/tcrv_edit.cpp
    src/tcrv_wnd_settings.cpp
    src/wxapp.cpp
)

target_include_directories(imppg PRIVATE src)

set(IMPPG_SHADERS_DIR ${CMAKE_INSTALL_FULL_DATAROOTDIR}/imppg/shaders)
set(IMPPG_IMAGES_DIR ${CMAKE_INSTALL_FULL_DATAROOTDIR}/imppg/images)
set(IMPPG_VERSION_MAJOR 2)
set(IMPPG_VERSION_MINOR 1)
set(IMPPG_VERSION_SUBMINOR 0)

target_compile_definitions(imppg PRIVATE
    IMPPG_VERSION_MAJOR=${IMPPG_VERSION_MAJOR}
    IMPPG_VERSION_MINOR=${IMPPG_VERSION_MINOR}
    IMPPG_VERSION_SUBMINOR=${IMPPG_VERSION_SUBMINOR}
)

set_compiler_options(imppg)

if(WIN32)
    # The RC file includes the application manifest and defines the application icon.
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        # 32-bit build
        target_sources(imppg PRIVATE src/imppg.rc src/app32.manifest)
    else()
        # 64-bit build
        target_sources(imppg PRIVATE src/imppg64.rc src/app64.manifest)
    endif()
endif()

find_package(Boost REQUIRED)
target_include_directories(imppg PRIVATE ${Boost_INCLUDE_DIRS})

set(wxWidgets_USE_UNICODE TRUE)
set(wxWidgets_USE_DEBUG FALSE)
if(UNIX AND NOT APPLE)
    set(wxWidgets_CONFIG_OPTIONS --toolkit=gtk3)
endif()

set(WX_COMPONENTS "adv aui xml core base richtext")
if(USE_OPENGL_BACKEND)
	string(APPEND WX_COMPONENTS " gl")
endif()

if(WIN32)
find_package(wxWidgets 3.1 REQUIRED COMPONENTS ${WX_COMPONENTS})
else()
find_package(wxWidgets REQUIRED COMPONENTS ${WX_COMPONENTS})
endif()

include(${wxWidgets_USE_FILE})
target_link_libraries(imppg PRIVATE ${wxWidgets_LIBRARIES})

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

if(USE_OPENGL_BACKEND)
    target_compile_definitions(imppg PRIVATE USE_OPENGL_BACKEND=1)
else()
    target_compile_definitions(imppg PRIVATE USE_OPENGL_BACKEND=0)
endif()

if(USE_CFITSIO EQUAL 1)
    pkg_check_modules(CFITSIO cfitsio REQUIRED)
    target_include_directories(imppg PRIVATE ${CFITSIO_INCLUDE_DIRS})
    target_link_libraries(imppg PRIVATE ${CFITSIO_LIBRARIES})
endif()

if(ENABLE_SCRIPTING EQUAL 1)
    target_compile_definitions(imppg PRIVATE ENABLE_SCRIPTING=1)
    target_sources(imppg PRIVATE src/script_dialog.cpp)
endif()

add_subdirectory(src/alignment)
add_subdirectory(src/math_utils)
add_subdirectory(src/common)
add_subdirectory(src/backend)
add_subdirectory(src/image)
add_subdirectory(src/logging)
if(ENABLE_SCRIPTING)
    add_subdirectory(src/scripting)
endif()

target_link_libraries(imppg PRIVATE alignment common math_utils backend image logging)
if(ENABLE_SCRIPTING EQUAL 1)
    target_link_libraries(imppg PRIVATE scripting)
endif()

# Installation -------------------------------------

install(TARGETS imppg DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})

install(FILES
    README.md
    README_pl.md
    main_wnd.png
    LICENSE
    DESTINATION ${CMAKE_INSTALL_FULL_DOCDIR}
)

install(FILES
    doc/scripting/api_reference.md
    doc/scripting/scripting.md
    DESTINATION ${CMAKE_INSTALL_FULL_DOCDIR}/doc/scripting
)

install(FILES
    shaders/copy.frag
    shaders/divide.frag
    shaders/gaussian_horz.frag
    shaders/gaussian_vert.frag
    shaders/mono_output_cubic.frag
    shaders/mono_output.frag
    shaders/multiply.frag
    shaders/pass-through.vert
    shaders/selection_outline.frag
    shaders/tone_curve.frag
    shaders/unsharp_mask.frag
    shaders/vertex.vert
    DESTINATION ${IMPPG_SHADERS_DIR}
)

install(FILES
    images/anim.bin
    images/crop.png
    images/fit_wnd.png
    images/imppg-app.ico
    images/imppg-app.png
    images/load_settings.png
    images/mru_settings.png
    images/open_file.png
    images/pad.png
    images/save_file.png
    images/save_settings.png
    images/select_all.png
    images/toggle_proc.png
    images/toggle_tcrv.png
    images/zoom_custom.png
    images/zoom_in.png
    images/zoom_none.png
    images/zoom_out.png
    DESTINATION ${IMPPG_IMAGES_DIR}
)

install(FILES lang/pl/imppg.mo DESTINATION ${CMAKE_INSTALL_FULL_LOCALEDIR}/pl/LC_MESSAGES)
install(FILES lang/ru/imppg.mo DESTINATION ${CMAKE_INSTALL_FULL_LOCALEDIR}/ru/LC_MESSAGES)
install(FILES lang/uk/imppg.mo DESTINATION ${CMAKE_INSTALL_FULL_LOCALEDIR}/uk/LC_MESSAGES)
install(FILES lang/de/imppg.mo DESTINATION ${CMAKE_INSTALL_FULL_LOCALEDIR}/de/LC_MESSAGES)
install(FILES lang/it/imppg.mo DESTINATION ${CMAKE_INSTALL_FULL_LOCALEDIR}/it/LC_MESSAGES)

file(WRITE imppg.desktop
"[Desktop Entry]
Type=Application
Version=1.0
Name=ImPPG
Comment=Image Post-Processor
Exec=${CMAKE_INSTALL_FULL_BINDIR}/imppg
Icon=${CMAKE_INSTALL_FULL_DATAROOTDIR}/imppg/images/imppg-app.png"
)

install(FILES
    imppg.desktop
    DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/applications
)

# Packaging -------------------------------------

# To create a binary package, add -DIMPPG_PKG_TYPE=<file name from packaging/> to CMake invocation,
# e.g., -DIMPPG_PKG_TYPE=ubuntu-22.04. Then build ImPPG and run `cpack`.
#
# Note that building needs to be performed in an environment corresponding to the selected target system,
# so that proper shared objects are linked to ("environment" = a full system installation, a Docker image, or similar).

if(NOT "${IMPPG_PKG_TYPE}" STREQUAL "")
    include(packaging/${IMPPG_PKG_TYPE}.cmake)
endif()
