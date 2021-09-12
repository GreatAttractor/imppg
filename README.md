# ImPPG (Image Post-Processor)
Copyright (C) 2015-2021 Filip Szczerek (ga.software@yahoo.com)

version 0.6.3 (2021-04-13)

*This program comes with ABSOLUTELY NO WARRANTY. This is free software, licensed under GNU General Public License v3 or any later version and you are welcome to redistribute it under certain conditions. See the LICENSE file for details.*


----------------------------------------

- 1\. Introduction
  - 1\.1. Processing and display back ends
- 2\. User interface features
- 3\. Supported image file formats
- 4\. Image processing
 - 4\.1\. Brightness normalization
 - 4\.2\. Lucy–Richardson deconvolution
 - 4\.3\. Unsharp masking
 - 4\.4\. Tone curve
- 5\. Saving/loading settings
- 6\. Batch processing
- 7\. Image sequence alignment
  - 7\.1\. High-contrast features stablization (phase correlation)
  - 7\.2\. Solar limb stabilization
- 8\. Misc
- 9\. Known problems
- 10\. Downloading
- 11\. Building from source code
  - 11\.1\. Building under Linux and similar systems using GNU (or compatible) toolchain
    - 11\.1\.1. Building under Ubuntu 18.04
    - 11\.1\.2. Packaging
  - 11\.2\. Building under MS Windows
  - 11\.3\. UI language
- 12\. Acknowledgements
- 13\. Change log

----------------------------------------

## Homepage: https://greatattractor.github.io/imppg/

![Main window](main_wnd.png)

----------------------------------------
## 1. Introduction

ImPPG (Image Post-Processor) performs common image post-processing tasks, in this order:

  - brightness levels normalization
  - non-blind Lucy–Richardson deconvolution with a Gaussian kernel
  - blurring or sharpening via unsharp masking
  - tone curve adjustment

(all the operations are optional). The processing is performed using 32-bit (single-precision) floating-point arithmetic. Settings for all the above steps can be saved to a file and used for batch processing of multiple images.

ImPPG can also align an image sequence, with possibly large and chaotic translations between images. This can be useful, for example, when preparing a solar time-lapse animation, where subsequent frames are offset due to inaccurate tracking of the telescope mount. Other possible applications are smoothing out of terrestrial landscape time-lapses or preparing raw frames (with serious image jitter) for stacking.

The alignment is performed (with sub-pixel accuracy) either via phase correlation, which automatically aligns on the most contrasty features, or by detecting and stabilizing the solar limb.


----------------------------------------
### 1.1. Processing and display back ends

Starting with version 0.6.0, ImPPG contains two processing & display back ends: "CPU + bitmaps" and "GPU (OpenGL)". The latter is enabled by default and offloads all processing (except image alignment), as well as view scrolling and scaling, onto the GPU. On most systems it is faster than CPU processing by a factor of several or more.

Processing back ends can be switched via menu `Settings/Back end`.


----------------------------------------
## 2. User interface features

  - The processing controls panel, initially docked on the left of the main window, can be undocked or docked on the right.

  - Both the processing controls panel and the tone curve editor can be closed and later restored via the "View" menu or toolbar buttons.

  - Changes to the L–R deconvolution, unsharp masking settings and the tone curve are applied only to the current selection within the image (bounded by an inverted-brightness or dashed frame). To process the whole image, use the `Select (and process) all` option from `Edit` menu or the corresponding toolbar button.

  - The current selection can be changed at any time by dragging the mouse with left button pressed. Changing the selection cancels all previously started processing (if any).

  - The smaller the selection, the faster the processing. For fine-tuning of L–R deconvolution’s *sigma*, it is recommended to use moderately small areas; this way moving the *sigma* slider will show the results almost instanteneously. The refreshing is slightly slower when the current zoom factor is not 100%.

  - The view can be zoomed in/out by using toolbar buttons, `View` menu items or mouse wheel.

  - The view can be scrolled by dragging with the middle or right mouse button.

  - Size of toolbar icons can be changed via the `Settings` menu.

----------------------------------------
## 3. Supported image file formats

Accepted input formats: BMP, JPEG, PNG, TIFF (most of bit depths and compression methods), TGA and other via the FreeImage library, FITS. Image is processed in grayscale and saved in one of the following formats: BMP 8-bit; PNG 8-bit; TIFF 8-bit, 16-bit, 32-bit floating-point (no compression or compressed with LZW or ZIP), FITS 8-bit, 16-bit or 32-bit floating-point.

Output images produces by the sequence alignment function are saved as uncompressed TIFF with number of channels and bit depth preserved (except 8-bit palettized ones; those are converted to 24-bit RGB). Input FITS files are saved as FITS with bit depth preserved.


----------------------------------------
## 4. Image processing

An illustrated processing tutorial can be found at http://greatattractor.github.io/imppg/tutorial/tutorial_en.html


----------------------------------------
### 4.1. Brightness normalization

ImPPG can automatically adjust the input image’s brightness levels (before all other processing steps) to fall into the user-supplied range of `min` to `max` (specified as percentage of the full brightness; 0% = black, 100% = white). The darkest input pixels will get the `min` value, the brightest: the `max` value.

Both `min` and `max` can be less than 0% and larger than 100% (this may clip the histogram). It is also allowed for `max` to be less than `min`; this will invert brightness levels (create a negative; also possible via tone curve adjustment, see sec. 4.4).

Normalization may be useful when processing a sequence of astronomical stack images captured during variable air transparency (i.e. with varying brightness levels).

Access by:
    menu: `Settings`/`Normalize brightness levels...`


----------------------------------------
### 4.2. Lucy–Richardson deconvolution

ImPPG performs image sharpening via non-blind Lucy–Richardson deconvolution using Gaussian kernel. The kernel’s width is specified by Gaussian *sigma*; increasing this value makes the sharpening coarser.

Recommended number of deconvolution iterations: 30 to 70. Specify 0 to disable L–R deconvolution.

The `Prevent ringing` checkbox enables an experimental function which reduces ringing (halo) around over-exposed (solid white) areas (e.g. a solar disc in a prominence image) caused by sharpening.

Access by:
    `Lucy–Richardson deconvolution` tab in the processing controls panel (on the left of the main window)


----------------------------------------
### 4.3. Unsharp masking

Unsharp masking can be used for final sharpening (independently of L–R deconvolution) or blurring of the image. The *sigma* parameter specifies the Gaussian kernel’s width; the larger the value, the coarser the sharpening or blurring. `Amount` specifies the effect’s strength. Value < 1.0 blurs the image, 1.0 does nothing, value > 1.0 sharpens the image.


#### Adaptive mode

In this mode the amount varies, depending on the input (unprocessed) image brightness. For areas darker than `threshold − transition width` the amount is set to `amount min`. For areas brighter than `threshold + transition width` the amount is set to `amount max`. Between those values the amount changes smoothly from `amount min` to `amount max`.

An example situation where this mode can be useful is stretching of faint solar prominences with the tone curve. Typically this will also underline the image noise (especially if L–R was applied), as the darker image areas have small signal-to-noise ratio. By setting `amount max` to a sharpening value (>1.0) appropriate for the solar disc interior, but `amount min` to 1.0 (i.e. no sharpening) and `threshold` to fall somewhere at the disc/prominences transition, the faint prominences and background are left unsharpened. They can be even smoothed out by setting `amount min` to <1.0, while keeping rest of the disc sharpened.

Is is also allowed to set `amount min` > `amount max`.

Access by:
    `Unsharp masking` box in the processing controls panel (on the left of the main window)


----------------------------------------
### 4.4. Tone curve

The tone curve editor allows changing the input-output mapping of brightness levels. The tone curve is only applied after all the other processing steps. The histogram shown in the editor window’s background corresponds to the output (just the current selection) of previous processing steps **before** the application of the tone curve.

Access by:
    menu: `View`/`Panels`/`Tone curve` or the corresponding toolbar button


----------------------------------------
## 5. Saving/loading settings

All settings described in **4.1-4.4** can be saved to/restored from a file (in XML format). A settings file is needed in order to use the batch processing function.

Access by:
    menu: `File` or the corresponding toolbar buttons


----------------------------------------
## 6. Batch processing

Multiple images can be processed using the same settings specified by an existing settings file (see sec. 5). Output images are saved in the specified folder under names appended with `_out`.

Access by:
    menu: `File`/`Batch processing...`


----------------------------------------
## 7. Image sequence alignment

The `Input image files` list in the `Image alignment` dialog contains the input files in the order they are to be aligned as a sequence. List items can be removed and shifted using the buttons in the upper right corner.

Enabling `Sub-pixel alignment` results in a smoother animation and less image drift. Saving of output files will be somewhat slower; sharp, 1-pixel details (if present) may get very slightly blurred.

The output files are saved under names suffixed with `_aligned`. FITS files are saved as FITS, the remaining formats: as TIFF files.


----------------------------------------
### 7.1. High-contrast features stabilization (phase correlation)

A general-purpose method. Attempts to keep the high-contrast features (e.g. sunspots, filaments, prominences, craters) stationary. In some cases it may be undesirable, e.g. in a multi-hour time-lapse of a sunspot nearing the solar disc’s limb; phase correlation would tend to keep the sunspot stationary, but not the limb.


----------------------------------------
### 7.2. Solar limb stabilization

Detects and matches the solar limb position. The more of the limb is visible in input images, the better the results. It is recommended that the images to align are already sharpened. Requirements:
  - the disc has to be brighter than the background
  - no vignetting or limb darkening exaggerated by post-processing
  - the disc must not be eclipsed by the Moon

Operations such as full/partial brightness inversion (creating a negative) or applying a “darkening” tone curve can be applied after the alignment.


Access by:
    menu: `Tools`/`Align image sequence...`


----------------------------------------
## 8. Misc

ImPPG stores certain settings (e.g. the main window’s size and position) in an INI file, whose location is platform-dependent. On recent versions of MS Windows the path is `%HOMEPATH%\AppData\Roaming\imppg.ini`, where `%HOMEPATH%` usually equals `C:\Users\<username>` (but if OneDrive is enabled, the file may be located there). On Linux the path is `~/.imppg`.


----------------------------------------
## 9. Known problems

  - ImPPG remembers and restores the positions and sizes of the main program window and the tone curve editor window. It may happen (rarely) the desktop environment interferes with window positioning and e.g. the tone curve window remains off-screen. In that case, use the menu command `Settings`/`Reset tone curve window position`, or delete the ImPPG configuration file (see section 8).

  - (OpenGL mode, MS Windows 8.1, Intel HD Graphics 5500, driver 10.18.14.5074) Cubic interpolation mode is broken (a shader compiler bug, or perhaps a texture read count limitation); use "View/Scaling method/Linear" instead. Note that the same hardware works fine under Linux (Fedora 29 5.3.11-100, Mesa 18.3.6).

  - (wxGTK 3.0.2, Fedora 20) All the toolbar buttons react correctly, but their displayed state may be incorrect after using the `View` menu items/close boxes of tone curve editor and processing controls panel. Cause: sometimes check tool’s Toggle() method does not work.

  - (wxGTK) As of 2/2015, some of GTK themes function incorrectly (e.g. “QtCurve”, but not “Raleigh”). In ImPPG this may manifest as follows:
    - `File open` dialog does not show any files after it is opened; the files do appear when the dialog is resized by the user
    - program crashes when trying to select the output directory in Batch dialog using the `Choose folder` dialog
    - program hangs when trying to change file type in the `Open image file` dialog

Solution: change the GTK theme to "Raleigh" (e.g. in Fedora use the "GTK+ Appearance" tool).


----------------------------------------
## 10. Downloading

ImPPG source code and binaries for MS Windows and Ubuntu 18.04 (x86-64) can be downloaded from:
    https://github.com/GreatAttractor/imppg/releases


----------------------------------------
## 11. Building from source code

Building from source code requires a C++ compiler toolchain (with C++17 support), CMake, Boost libraries v. 1.57.0 or later (though earlier versions may work) and wxWidgets 3.0 (3.1 under MS Windows). Support for more image formats requires the FreeImage library, version 3.14.0 or newer. Without FreeImage the only supported formats are: BMP 8-, 24- and 32-bit, TIFF mono and RGB, 8 or 16 bits per channel (no compression). FITS support (optional) requires the CFITSIO library. Multithreaded processing requires a compiler supporting OpenMP.

To enable/disable usage of CFITSIO, FreeImage and GPU/OpenGL back end (they are enabled by default), edit the `config.in` file.

To remove any created CMake configuration, delete `CMakeCache.txt` and the `CMakeFiles` folder.


### 11.1. Building under Linux and similar systems using GNU (or compatible) toolchain

*Note: CMake relies on the presence of the `wx-config` tool to detect and configure wxWidgets-related build options. Sometimes this tool can be named differently, e.g. in Fedora 23 with wxGTK3 packages from repository it is `wx-config-3.0`. Older versions of CMake may not accept it. This can be remedied e.g. by creating a symlink:*
```bash
$ sudo ln -s /usr/bin/wx-config-3.0 /usr/bin/wx-config
```

Download the source code manually or clone with Git:
```bash
$ git clone https://github.com/GreatAttractor/imppg.git
```

In the source folder, run:
```bash
$ mkdir build
$ cd build
$ cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ..
```
This creates a native `Makefile`. Unless you edit `config.in` again, from now on there is no need to run CMake.

To compile ImPPG, run:
```bash
$ make
```
You will find `imppg` executable in the `build` folder.

To install ImPPG system-wide, run from the `build` folder:
```bash
$ sudo cmake -P cmake_install.cmake
```

To uninstall, run:
```bash
$ cat install_manifest.txt | sudo xargs rm
```

To use a different installation prefix, add `-DCMAKE_INSTALL_PREFIX=<my_dir>` to the initial CMake invocation.

#### 11.1.1. Building under Ubuntu 18.04

The following packages are needed for building under Ubuntu 18.04:
```
git cmake build-essential libboost-dev libwxgtk3.0-gtk3-dev libglew-dev pkg-config libccfits-dev libfreeimage-dev
```

The default GCC version (7.x) is too old. Install and enable GCC 8 (example instructions: `https://linuxize.com/post/how-to-install-gcc-compiler-on-ubuntu-18-04/`). (Do not choose GCC 9, otherwise the built binary will not run on a clean Ubuntu 18.04 due to too old a version of `libstdc++`.)

After building `imppg`, it can be either installed as in section 11.1, or a Debian package can be created and installed with `apt` (see 11.1.2).


#### 11.1.2. Packaging

In order to create a binary package, modify appropriately the final statement in `CMakeLists.txt`:
```cmake
include(packaging/ubuntu_20.04.cmake)
```
before calling CMake and building (see the `packaging` subdirectory for possible targets). After a successful build, the package can be created by running:
```bash
$ cpack
```
Note that building needs to be performed in an environment corresponding to the selected target system, so that proper shared objects are linked to (“environment” = a full system installation, a Docker image, or similar).

#### 11.1.3. Building for macOS

*Note: macOS build and support is still work-in-progress*

To build ImPPG for macOS you will need Xcode and [Homebrew](https://brew.sh) installed.

OpenMP is not supported yet as Apple has disabled OpenMP in clang toolchain distributed with Xcode. At this writing the build method has been verified on macOS Big Sur 11.5.2 and Xcode 12.5.1.

Install following libraries with Homebrew:
```bash
$ brew update
$ brew install cmake freeimage glew wxwidgets cfitsio
```

Now follow Linux build steps:
```bash
$ mkdir build
$ cd build
$ cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ..
$ make -j8
```

----------------------------------------
### 11.2. Building under MS Windows

The provided `CMakeLists.txt` supports the [MSYS2](http://www.msys2.org/) build environment. With manual configuration, other toolchains can also be used (e.g. MS Visual Studio).

In order to build ImPPG (64-bit) under MSYS2, follow its installation instructions at http://www.msys2.org/. Then open the MSYS2/MinGW64 shell (after default installation: `c:\msys64\mingw64.exe`) and install the ImPPG's dependencies by running:
```bash
$ pacman -S git mingw-w64-x86_64-cmake base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-boost mingw-w64-x86_64-cfitsio mingw-w64-x86_64-freeimage mingw64/mingw-w64-x86_64-glew mingw64/mingw-w64-x86_64-wxmsw3.1
```

Download the ImPPG's source code manually or clone it with Git:
```bash
$ git clone https://github.com/GreatAttractor/imppg.git
```

In the ImPPG source folder, run:
```bash
$ mkdir build
$ cd build
$ cmake -G "MSYS Makefiles" -DCMAKE_MAKE_PROGRAM=mingw32-make -DCMAKE_BUILD_TYPE=Release ..
```
This creates a native `Makefile`. Unless you edit `config.in` again, from now on there is no need to run CMake.

To compile ImPPG, run:
```bash
$ mingw32-make
```

You will find `imppg.exe` executable in the `build` folder. It can be run from MSYS2 shell, from the ImPPG source folder:
```bash
$ build/imppg.exe
```

To run ImPPG from Windows Explorer, the subfolders `images`, `pl`, `shaders` and all the necessary DLLs must be placed at the same location as `imppg.exe`. See the MS Windows binary distribution (`imppg-win64.zip`) for an example.


----------------------------------------
### 11.3. UI language


ImPPG supports multiple user interface languages using the wxWidgets built-in internationalization facilities. All translatable strings in the source code are surrounded by the `_()` macro. Adding a new translation requires the `GNU gettext` package and consists of the following steps:

- extraction of translatable strings from sources into a PO file by running:
```bash
$ xgettext -k_ src/*.cpp src/*.h -o imppg.po
```

- translation of UI strings by editing the `msgstr` entries in `imppg.po`

- converting `imppg.po` to binary form by running:
```bash
$ msgfmt imppg.po -o imppg.mo
```

- placing `imppg.mo` in a subdirectory with name equal to the language code (e.g. `pl`, `fr-ca`)

- adding the language to the arrays inside `c_MainWindow::SelectLanguage()` (`main_window.cpp`)

Binary distribution of ImPPG needs only the MO (binary) language files. Beside the `imppg.mo` file(s), also the wxWidgets translation is needed (for strings like standard menu items, e.g. “Open”; control captions, e.g. “Browse” etc.). Those can be found in `<wxWidgets_source_root>/locale`. On Windows, the wxWidgets file `<language>.mo` has to be available as `<imppg_root_folder>/<language>/wxstd3.mo`. On operating systems with a common location for all `.mo` files (e.g., Linux), it is sufficient to have a wxWidgets installation.


----------------------------------------
## 12. Acknowledgements

Russian and Ukrainian translations: Rusłan Pazenko.


----------------------------------------
## 13. Change log

**0.6.3** (2021-04-13)

  - **Enhancements**
    - Opening an image file by dragging and dropping onto the ImPPG window

  - **Bug fixes**
    - Crash when batch processing in CPU mode with deringing enabled

**0.6.2** (2020-07-04)

  - **New features**
    - Russian and Ukrainian translations

**0.6.1** (2020-01-25)

  - **Bug fixes**
    - Invalid batch processing results in OpenGL mode

  - **Enhancements**
    - Tone curve window position reset command

**0.6.0** (2019-12-23)

  - **New features**
    - GPU (OpenGL) back end for much faster processing

  - **Enhancements**
    - View scrolling by dragging with the right mouse button (previously: with the middle button)
    - Zooming in/out with the mouse wheel (previously: Ctrl + mouse wheel)

**0.5.4** (2019-02-02)

  - **New features**
    - Configurable look of the tone curve editor

  - **Enhancements**
    - Last loaded settings file name shown in toolbar

**0.5.3** (2017-03-12)

  - **Bug fixes**
    - Fixed filling the list of recently used settings under Windows

**0.5.2** (2017-01-07)

  - **Bug fixes**
    - Fixed drawing of histogram and tone curve

**0.5.1** (2016-10-02)

  - **New features**
    - List of most recently used settings files

  - **Enhancements**
    - High-resolution toolbar icons
    - Improved tone curve drawing performance on high-resolution displays

**0.5** (2016-01-02)

  - **New features**
    - Adaptive unsharp masking

  - **Enhancements**
    - Numerical sliders can be scrolled with cursor keys
    - Processing panel width is preserved
    - Using CMake for building

**0.4.1** (2015-08-30)

  - **Enhancements**
    - Numerical sliders use 1-pixel steps instead of hard-coded 100 steps
    - Output format selected in batch processing dialog is preserved
    - Unsharp masking not slowing down for large values of *sigma*
    - Increased the range of unsharp masking parameters

  - **Bug fixes**
    - Invalid output file name after alignment if there was more than one period in input name
    - Crash when a non-existing path is entered during manual editing
    - Program windows placed outside the screen when ImPPG was previously run on multi-monitor setup
    - Restored missing Polish translation strings

**0.4** (2015-06-21)

  - **New features**
    - Image sequence alignment via solar limb stabilization
    - FITS files support (load/save)
    - Zooming in/out of the view

  - **Enhancements**
    - View scrolling by dragging with the middle mouse button
    - Logarithmic histogram setting is preserved

  - **Bug fixes**
    - Tone curve in gamma mode not applied during batch processing

**0.3.1** (2015-03-22)

  - **New features**
    - Polish translation; added instructions for creating additional translations

**0.3** (2015-03-19)

  - **New features**
    - Image sequence alignment via phase correlation

  - **Enhancements**
    - Limited the frequency of processing requests to improve responsiveness during changing of unsharp masking parameters and editing of tone curve

  - **Bug fixes**
    - Incorrect output file extension after batch processing when the selected output format differs from the input

**0.2** (2015-02-28)

  - **New features**
    - Support for more image file formats via FreeImage. New output formats: PNG/8-bit, TIFF/8-bit LZW-compressed, TIFF/16-bit ZIP-compressed, TIFF/32 bit floating-point (no compression and ZIP-compressed).

  - **Enhancements**
    - Enabled the modern-look GUI controls on Windows

  - **Bug fixes**
    - Selection border not marked on platforms w/o logical raster ops support (e.g. GTK 3)

**0.1.1** (2015-02-24)

  - **Bug fixes**
    - Blank output files after batch processing when L–R iteration count is 0

**0.1** (2015-02-21)

  - **Initial revision**
