#
# ImPPG build file
#
# Prerequisite libraries: wxWidgets 3.0, Boost 1.57.0, FreeImage (optional) 3.14.0 or newer, CFITSIO (optional).
# Before building, edit config.in to configure optional libs and to set include and library paths
# if building on MS Windows.
#
# NOTE: On systems other than MS Windows, CMake relies on the presence of the "wx-config" tool
#       to detect and configure wxWidgets-related build options. Sometimes this tool can be
#       named differently, e.g. in Fedora 23 with wxGTK3 packages from repository it is
#       "wx-config-3.0". This can be remedied e.g. by creating a symlink:
#
#         sudo ln -s /usr/bin/wx-config-3.0 /usr/bin/wx-config
#

project("ImPPG")

cmake_minimum_required(VERSION 3.0)

include(config.in)

if(WIN32)
    link_directories("${WX_LIB_DIR}")

    if(USE_FREEIMAGE EQUAL 1)
        link_directories("${FREEIMAGE_LIB_DIR}")
    endif()

    if(USE_CFITSIO EQUAL 1)
        link_directories("${CFITSIO_LIB_DIR}")
    endif()
endif()
add_executable(imppg WIN32
    about.cpp
    align_disc.cpp
    align_params.cpp
    align_phasecorr.cpp
    align_proc.cpp
    align_progress.cpp
    appconfig.cpp
    batch.cpp
    batch_params.cpp
    bmp.cpp
    common.cpp
    cursors.cpp
    formats.cpp
    fft.cpp
    gauss.cpp
    image.cpp
    logging.cpp
    lrdeconv.cpp
    main.cpp
    main_window.cpp
    normalize.cpp
    num_ctrl.cpp
    scrollable_dlg.cpp
    settings.cpp
    tcrv.cpp
    tcrv_edit.cpp
    tiff.cpp
    w_lrdeconv.cpp
    w_tcurve.cpp
    w_unshmask.cpp
    worker.cpp
    wxapp.cpp
)

# The imppg(64).rc files include the manifest that enables MS Windows Common Controls 6.0.0.0
# (for Windows XP and above), i.e. the nicer-looking, themed ones. This is needed only for MinGW,
# MSVC generates and embeds the manifest automatically.
if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINK_FLAGS} -Wl,--strip-all")

    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        # 32-bit build
        target_sources(imppg PUBLIC imppg.rc)
    else()
        # 64-bit build
        target_sources(imppg PUBLIC imppg64.rc)

        # option required by wxWidgets 3.0.2 as of g++ 5.2.0
        target_compile_options(imppg PUBLIC "-std=gnu++11")
    endif()
endif()

find_package(Boost REQUIRED)
target_include_directories(imppg PUBLIC ${Boost_INCLUDE_DIRS})

if(NOT WIN32)

    # wxWidgets is found via wx-config (e.g. on Linux)
    find_package(wxWidgets REQUIRED COMPONENTS adv aui xml core base)
    set(wxWidgets_USE_UNICODE TRUE)
    set(wxWidgets_USE_DEBUG FALSE)
    include(${wxWidgets_USE_FILE})
    target_link_libraries(imppg ${wxWidgets_LIBRARIES})
    
else()

    # I couldn't get FindwxWidgets.cmake module to work as of CMake 3.4.1/wxWidgets 3.0.2/MinGW-64 (g++ 5.2.0)
    # (though it worked fine for MinGW 32-bit libs) and didn't spend too much time debugging it.
    # For now let's specify everything manually instead.
    message("Using user-supplied wxWidgets paths.")
    target_include_directories(imppg PUBLIC "${WX_INCLUDE_DIR}")
    target_include_directories(imppg PUBLIC "${WX_LIB_DIR}\\mswu") # needed for setup.h
    add_definitions(-D_UNICODE -D__WXMSW__)
    target_link_libraries(imppg
        wxmsw30u_aui
        wxmsw30u_adv
        wxmsw30u_core
        wxbase30u_xml
        wxexpat
        wxbase30u
        wxtiff
        wxjpeg
        wxpng
        wxzlib
        rpcrt4
        oleaut32
        ole32
        uuid
        winspool
        winmm
        shell32
        comctl32
        comdlg32
        advapi32
        wsock32
        gdi32
        user32
    )
endif()

find_package(OpenMP)
if(OPENMP_FOUND)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

set(CMAKE_BUILD_TYPE Release)

if (NOT MSVC)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(-ffast-math GNU_FFAST_MATH_SUPPORTED)
endif()

if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast")
elseif(GNU_FFAST_MATH_SUPPORTED)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")
endif()

if(USE_FREEIMAGE EQUAL 1)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_FREEIMAGE")
    if(WIN32)
        target_include_directories(imppg PUBLIC "${FREEIMAGE_INCLUDE_DIR}")
    endif()
    target_link_libraries(imppg freeimage)
    
endif()

if(USE_CFITSIO EQUAL 1)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_CFITSIO")
    if(WIN32)
        target_include_directories(imppg PUBLIC "${CFITSIO_INCLUDE_DIR}")
    endif()
    target_link_libraries(imppg cfitsio)
endif()
