# ImPPG scripting API reference

## Modules

- [imppg](#moduleimppg)
- [imppg.filesystem](#imppgfilesystem)

### `imppg`

- `load_image_as_mono32f`

  Loads an image as 32-bit floating-point monochrome.

  *Parameters:*
  - path to image file

  ----
  *Example:*
  ```Lua
  image = imppg.load_image_as_mono32f("/path/to/image.tif")
  ```

- `new_settings`

  Creates a [settings](#settings) object which does not introduce any image changes when applied (i.e., disabled sharpening, identity tone curve).

  *Parameters*: none

  ----
  *Example:*
  ```Lua
  settings = imppg.new_settings()
  ```

### `imppg.filesystem`

- `list_files`

  Creates an iterator over all files matching the file path mask. The iteration is performed in an unspecified order.

  *Parameters:*
  - file path mask (wildcards are supported)

  ----
  *Example:*
  ```Lua
  -- iterate over all files in "my_folder"
  for f in imppg.filesystem.list_files("/my_folder") do
    print(f)
  end
  ```
  ```Lua
  -- iterate over all TIFF files in "my_folder"
  for f in imppg.filesystem.list_files("/my_folder/*.tif") do
    print(f)
  end
  ```
  ```Lua
  -- iterate over all TIFF files in "my_folder" which start with "prefix"
  for f in imppg.filesystem.list_files("/my_folder/prefix*.tif") do
    print(f)
  end
  ```

## Classes

### `settings`

Methods:

- `get_unsh_mask_sigma`

  Returns the unsharp mask *sigma*.

  *Parameters:* none

  ----
  *Example:*

  ```Lua
  s = imppg.new_settings()
  print(s:get_unsh_mask_sigma())
  ```

- `get_unsh_mask_amount`

  Returns the unsharp mask *amount*.

  *Parameters:* none

  ----
  *Example:*

  ```Lua
  s = imppg.new_settings()
  print(s:get_unsh_mask_amount())
  ```