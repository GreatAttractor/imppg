# ImPPG scripting API reference

## Modules

- [imppg](#moduleimppg)
- [imppg.filesystem](#imppgfilesystem)

### `imppg`

- `load_image`

  Loads a monochrome or RGB image; the resulting object can be passed to image processing functions.

  *Parameters:*
  - image file path

  ----
  *Example:*
  ```Lua
  image = imppg.load_image("/path/to/image.tif")
  ```

- `new_settings`

  Creates a [settings](#settings) object which does not introduce any image changes when applied (i.e., disabled sharpening, identity tone curve).

  *Parameters*: none

  ----
  *Example:*
  ```Lua
  settings = imppg.new_settings()
  ```

- `process_image_file`

  Processes an image file according to a settings file and saves the result.
  **TODO** specifying output format

  *Parameters*:
  - image file path
  - settings file path
  - processed image file path

  ----
  *Example*
  ```Lua
  imppg.process_image_file("/path/to/image.tif", "/path/to/settings.xml", "/path/to/output.tif")
  ```

- `process_image`

  Processes image according to given settings and returns the result.

  *Parameters*:
  - image
  - settings

  ----
  *Example*
  ```Lua
  settings = imppg.new_settings()
  settings.unsh_mask_sigma(1.5)
  image = imppg.load_image("/path/to/image.tif")
  processed_image = imppg.process_image(image, settings)
  ```

### `imppg.filesystem`

- `list_files`

  Returns an iterator over all files matching the file path mask. The iteration is performed in an unspecified order.

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

- `list_files_sorted`

  Returns a sorted table (and its length) of files matching the file path mask (see `list_files` examples of masks).

  *Parameters:*
  - file path mask (wildcards are supported)



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