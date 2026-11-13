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

# Embed the dynamic libraries the macOS application links against into the
# ImPPG.app bundle (Contents/Frameworks/), so the bundle runs on machines
# without Homebrew (or equivalent libraries) installed. Requires the
# "dylibbundler" tool (brew install dylibbundler); the resulting bundle is
# re-signed ad-hoc, or with $IMPPG_CODESIGN_IDENTITY when that environment
# variable is set at build time. See scripts/macos/bundle-dylibs.sh.
option(IMPPG_BUNDLE_DYLIBS "macOS: embed dependent dylibs into the app bundle, making it self-contained" OFF)

include(config.cmake)
include(utils.cmake)
include(FindPkgConfig)
include(GNUInstallDirs)

# On macOS the executable is built as an .app bundle: MACOSX_BUNDLE makes CMake
# produce "ImPPG.app" with the executable in Contents/MacOS/ and Info.plist in
# Contents/. The resource lookup code (src/common/src/dirs.cpp and
# src/backend/src/opengl/gl_utils.cpp) already falls back to Contents/Resources/.
# (The WIN32 and MACOSX_BUNDLE keywords each only take effect on their platform.)
add_executable(imppg WIN32 MACOSX_BUNDLE
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

if(APPLE)
    # Properties of the ImPPG.app bundle. The icon is generated from
    # images/imppg-app.png by the "imppg-icon" custom target below.
    set_target_properties(imppg PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.github.GreatAttractor.imppg"
        MACOSX_BUNDLE_BUNDLE_NAME "ImPPG"
        # Bundle directory and executable are named "ImPPG" (the install rules
        # and the DMG packaging below rely on this name).
        OUTPUT_NAME "ImPPG"
        MACOSX_BUNDLE_EXECUTABLE_NAME "ImPPG"
        MACOSX_BUNDLE_ICON_FILE "imppg-app.icns"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${IMPPG_VERSION_MAJOR}.${IMPPG_VERSION_MINOR}.${IMPPG_VERSION_SUBMINOR}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${IMPPG_VERSION_MAJOR}.${IMPPG_VERSION_MINOR}.${IMPPG_VERSION_SUBMINOR}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${IMPPG_VERSION_MAJOR}.${IMPPG_VERSION_MINOR}.${IMPPG_VERSION_SUBMINOR}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright (C) 2016-2026, Filip Szczerek"
        # CMake's default MacOSXBundleInfo.plist.in lacks NSHighResolutionCapable
        # (needed for Retina support), so use a custom template.
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/ImPPG-Info.plist.in"
    )
endif()

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
    # On macOS, AppleClang does not link the OpenMP runtime (libomp) implicitly
    # (GCC on Linux links libgomp automatically). Add it to the global linker
    # flags so all executables (imppg and tests) get it.
    if(APPLE AND OpenMP_libomp_LIBRARY)
        get_filename_component(IMPPG_OMP_LIB_DIR "${OpenMP_libomp_LIBRARY}" DIRECTORY)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${IMPPG_OMP_LIB_DIR} -lomp")
    endif()
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

if(APPLE)
    # Generate imppg-app.icns from images/imppg-app.png using only tools shipped
    # with macOS (sips + iconutil).
    set(IMPPG_ICONSET_DIR ${CMAKE_CURRENT_BINARY_DIR}/imppg-app.iconset)
    set(IMPPG_ICNS_FILE ${CMAKE_CURRENT_BINARY_DIR}/imppg-app.icns)
    add_custom_command(
        OUTPUT ${IMPPG_ICNS_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${IMPPG_ICONSET_DIR}
        COMMAND sips -z 16 16 ${CMAKE_CURRENT_SOURCE_DIR}/images/imppg-app.png --out ${IMPPG_ICONSET_DIR}/icon_16x16.png
        COMMAND sips -z 32 32 ${CMAKE_CURRENT_SOURCE_DIR}/images/imppg-app.png --out ${IMPPG_ICONSET_DIR}/icon_16x16@2x.png
        COMMAND sips -z 32 32 ${CMAKE_CURRENT_SOURCE_DIR}/images/imppg-app.png --out ${IMPPG_ICONSET_DIR}/icon_32x32.png
        COMMAND sips -z 64 64 ${CMAKE_CURRENT_SOURCE_DIR}/images/imppg-app.png --out ${IMPPG_ICONSET_DIR}/icon_32x32@2x.png
        COMMAND sips -z 128 128 ${CMAKE_CURRENT_SOURCE_DIR}/images/imppg-app.png --out ${IMPPG_ICONSET_DIR}/icon_128x128.png
        COMMAND sips -z 256 256 ${CMAKE_CURRENT_SOURCE_DIR}/images/imppg-app.png --out ${IMPPG_ICONSET_DIR}/icon_128x128@2x.png
        COMMAND sips -z 256 256 ${CMAKE_CURRENT_SOURCE_DIR}/images/imppg-app.png --out ${IMPPG_ICONSET_DIR}/icon_256x256.png
        COMMAND sips -z 512 512 ${CMAKE_CURRENT_SOURCE_DIR}/images/imppg-app.png --out ${IMPPG_ICONSET_DIR}/icon_256x256@2x.png
        COMMAND iconutil -c icns -o ${IMPPG_ICNS_FILE} ${IMPPG_ICONSET_DIR}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/images/imppg-app.png
        VERBATIM
        COMMENT "Generating imppg-app.icns"
    )
    add_custom_target(imppg-icon ALL
        DEPENDS ${IMPPG_ICNS_FILE}
    )
    set_target_properties(imppg PROPERTIES
        MACOSX_BUNDLE_ICON_FILE imppg-app.icns
    )
    set_source_files_properties(${IMPPG_ICNS_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(imppg PRIVATE ${IMPPG_ICNS_FILE})

    # Copy the runtime resources into the build-tree bundle so imppg.app is
    # directly runnable after "make" (dirs.cpp/gl_utils.cpp look in
    # Contents/Resources/ when launched from the bundle).
    # Translations use Apple's <lang>.lproj/LC_MESSAGES layout, which is what
    # wx's translation loader searches for on macOS.
    set(IMPPG_BUNDLE_RES $<TARGET_FILE_DIR:imppg>/../Resources)
    add_custom_command(TARGET imppg POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders
            ${IMPPG_BUNDLE_RES}/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/images
            ${IMPPG_BUNDLE_RES}/images
        COMMAND ${CMAKE_COMMAND} -E make_directory
            ${IMPPG_BUNDLE_RES}/pl.lproj/LC_MESSAGES
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/lang/pl/imppg.mo
            ${IMPPG_BUNDLE_RES}/pl.lproj/LC_MESSAGES/imppg.mo
        COMMAND ${CMAKE_COMMAND} -E make_directory
            ${IMPPG_BUNDLE_RES}/ru.lproj/LC_MESSAGES
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/lang/ru/imppg.mo
            ${IMPPG_BUNDLE_RES}/ru.lproj/LC_MESSAGES/imppg.mo
        COMMAND ${CMAKE_COMMAND} -E make_directory
            ${IMPPG_BUNDLE_RES}/uk.lproj/LC_MESSAGES
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/lang/uk/imppg.mo
            ${IMPPG_BUNDLE_RES}/uk.lproj/LC_MESSAGES/imppg.mo
        COMMAND ${CMAKE_COMMAND} -E make_directory
            ${IMPPG_BUNDLE_RES}/de.lproj/LC_MESSAGES
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/lang/de/imppg.mo
            ${IMPPG_BUNDLE_RES}/de.lproj/LC_MESSAGES/imppg.mo
        COMMAND ${CMAKE_COMMAND} -E make_directory
            ${IMPPG_BUNDLE_RES}/it.lproj/LC_MESSAGES
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/lang/it/imppg.mo
            ${IMPPG_BUNDLE_RES}/it.lproj/LC_MESSAGES/imppg.mo
        COMMENT "Copying resources into imppg.app"
    )

    # Make the bundle self-contained by embedding the dylibs it links against
    # (Contents/Frameworks/). Runs only on relink, when the executable is fresh;
    # the script refuses an already-bundled one (it cannot be re-processed).
    if(IMPPG_BUNDLE_DYLIBS)
        add_custom_command(TARGET imppg POST_BUILD
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/macos/bundle-dylibs.sh $<TARGET_FILE_DIR:imppg>/../..
            COMMENT "Bundling dylibs into imppg.app"
        )
    endif()
endif()

# Installation -------------------------------------

if(APPLE)
    # Resources go into the .app bundle's Contents/Resources/ (both the build
    # tree bundle, via the POST_BUILD rules, and the installed bundle).
    # Translations use Apple's <lang>.lproj/LC_MESSAGES layout, which is what
    # wx's translation loader searches for on macOS.
    set(IMPPG_INSTALL_DOCDIR "ImPPG.app/Contents/Resources/doc")
    set(IMPPG_INSTALL_SHADERSDIR "ImPPG.app/Contents/Resources/shaders")
    set(IMPPG_INSTALL_IMAGESDIR "ImPPG.app/Contents/Resources/images")
    set(IMPPG_INSTALL_LOCALEDIR "ImPPG.app/Contents/Resources")
else()
    set(IMPPG_INSTALL_DOCDIR ${CMAKE_INSTALL_DOCDIR})
    set(IMPPG_INSTALL_SHADERSDIR ${CMAKE_INSTALL_DATAROOTDIR}/imppg/shaders)
    set(IMPPG_INSTALL_IMAGESDIR ${CMAKE_INSTALL_DATAROOTDIR}/imppg/images)
    set(IMPPG_INSTALL_LOCALEDIR ${CMAKE_INSTALL_LOCALEDIR})
endif()

install(TARGETS imppg
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    README.md
    README_pl.md
    main_wnd.png
    LICENSE
    DESTINATION ${IMPPG_INSTALL_DOCDIR}
)

install(FILES
    doc/scripting/api_reference.md
    doc/scripting/scripting.md
    DESTINATION ${IMPPG_INSTALL_DOCDIR}/doc/scripting
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
    DESTINATION ${IMPPG_INSTALL_SHADERSDIR}
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
    DESTINATION ${IMPPG_INSTALL_IMAGESDIR}
)

if(APPLE)
    # Translations use Apple's convention: Resources/<lang>.lproj/LC_MESSAGES/.
    foreach(IMPPG_LANG pl ru uk de it)
        install(FILES lang/${IMPPG_LANG}/imppg.mo
            DESTINATION ${IMPPG_INSTALL_LOCALEDIR}/${IMPPG_LANG}.lproj/LC_MESSAGES)
    endforeach()
else()
    install(FILES lang/pl/imppg.mo DESTINATION ${IMPPG_INSTALL_LOCALEDIR}/pl/LC_MESSAGES)
    install(FILES lang/ru/imppg.mo DESTINATION ${IMPPG_INSTALL_LOCALEDIR}/ru/LC_MESSAGES)
    install(FILES lang/uk/imppg.mo DESTINATION ${IMPPG_INSTALL_LOCALEDIR}/uk/LC_MESSAGES)
    install(FILES lang/de/imppg.mo DESTINATION ${IMPPG_INSTALL_LOCALEDIR}/de/LC_MESSAGES)
    install(FILES lang/it/imppg.mo DESTINATION ${IMPPG_INSTALL_LOCALEDIR}/it/LC_MESSAGES)
endif()

if(UNIX AND NOT APPLE)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/imppg.desktop
"[Desktop Entry]
Type=Application
Version=1.0
Name=ImPPG
Comment=Image Post-Processor
Exec=${CMAKE_INSTALL_FULL_BINDIR}/imppg
Icon=${CMAKE_INSTALL_FULL_DATAROOTDIR}/imppg/images/imppg-app.png"
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/imppg.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)
endif()

# Packaging -------------------------------------

# To create a binary package, add -DIMPPG_PKG_TYPE=<file name from packaging/> to CMake invocation,
# e.g., -DIMPPG_PKG_TYPE=ubuntu-22.04. Then build ImPPG and run `cpack`.
#
# Note that building needs to be performed in an environment corresponding to the selected target system,
# so that proper shared objects are linked to ("environment" = a full system installation, a Docker image, or similar).

if(APPLE AND "${IMPPG_PKG_TYPE}" STREQUAL "")
    set(IMPPG_PKG_TYPE macos)
endif()

if(NOT "${IMPPG_PKG_TYPE}" STREQUAL "")
    include(packaging/${IMPPG_PKG_TYPE}.cmake)
endif()

if(APPLE AND "${IMPPG_PKG_TYPE}" STREQUAL "macos")
    add_custom_target(imppg-dmg ALL
        COMMAND ${CMAKE_CPACK_COMMAND} --config ${CMAKE_CURRENT_BINARY_DIR}/CPackConfig.cmake
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS imppg
        COMMENT "Creating the macOS DMG"
        VERBATIM
    )
endif()
