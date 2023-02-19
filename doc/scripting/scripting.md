# Introduction

All ImPPG's image processing facilities can be invoked from a script written in the [Lua](https://www.lua.org/docs.html) programming language. For that purpose, ImPPG exposes an application programming interface (API); see [here](api_reference.md) for its reference.

A script can be loaded and run via the `File` / `Run script...` menu option.

# Examples

## Process and save a single image using an existing settings file

```Lua
imppg.process_image_file("/path/to/image.tif", "/path/to/settings.xml", "/path/to/output.png", imppg.PNG_8)
```

## Process multiple images using the same settings

```Lua
settings = imppg.load_settings("/path/to/settings.xml")
files, num_files = imppg.filesystem.list_files_sorted("/path/to/stack*.tif")
for index, file in ipairs(files) do
    image = imppg.load_image(file)
    processed_image = imppg.process_image(image, settings)
    processed_image:save(string.format("/path/to/processed/output_%02d.png", index), imppg.PNG_8)
    imppg.progress(index / num_files)
end
```

## Process an image using varying settings

```Lua
settings = imppg.load_settings("/path/to/settings.xml")
image = imppg.load_image("/path/to/stack.tif")

-- use sigma from 1.0 to 1.4, increasing by 0.1
for sigma = 1.0, 1.5, 0.1 do
    settings:lr_deconv_sigma(sigma)
    processed_image = imppg.process_image(image, settings)
    processed_image:save(string.format("/path/to/processed/output_%0.1f.png", sigma), imppg.PNG_8)
end
```

# Remarks


## Script progress

The progress bar state in "Run script" dialog can be set by calling `imppg.progress(fraction)` (where `fraction` is between 0.0 and 1.0). It is up to the script to choose a meaningful value.

## Long paths

When operating on files located in a deeply nested path, Lua's string concatenation can be used to avoid repetition. Instead of:

```Lua
settings = imppg.load_settings("/path/to/settings.xml")

image = imppg.load_image("/very/long/deeply/nested/path/to/input/image01.tif")
proc = imppg.process_image(image, settings)
proc:save("/very/long/deeply/nested/path/to/output/output01.png", imppg.PNG_8)

image = imppg.load_image("/very/long/deeply/nested/path/to/input/image02.tif")
proc = imppg.process_image(image, settings)
proc:save("/very/long/deeply/nested/path/to/output/output02.png", imppg.PNG_8)
```

one can write:

```Lua
dir = "/very/long/deeply/nested/path/to/";

settings = imppg.load_settings("/path/to/settings.xml")

image = imppg.load_image(dir .. "input/image01.tif")
proc = imppg.process_image(image, settings)
proc:save(dir .. "output/output01.png", imppg.PNG_8)

image = imppg.load_image(dir .. "input/image02.tif")
proc = imppg.process_image(image, settings)
proc:save(dir .. "output/output02.png", imppg.PNG_8)
```

## Interrupting script execution

Script execution can be interrupted using the `Stop` button in the `Run script` dialog. The interruption happens on a subsequent script call to any ImPPG API function/method. If the script does not make such a call, and instead e.g. hangs in an infinite loop (`while true do end`), the user needs to kill the whole ImPPG process.