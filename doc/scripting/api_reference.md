# ImPPG scripting API reference

## Remarks

Scripts can only operate on file paths which are convertible to UTF-8.

## Modules

- [imppg](#moduleimppg)
- [imppg.filesystem](#imppgfilesystem)

### Module `imppg`

- `load_image`

  Loads a monochrome or RGB image; the resulting object can be passed to image processing functions.

  *Parameters:*
  - image file path

  ----
  *Example:*
  ```Lua
  image = imppg.load_image("/path/to/image.tif")
  ```

- `load_settings`

  Loads processing settings from a file; the resulting [settings](#settings) object can be passed to image processing functions.

  *Parameters:*
  - settings file path

  ----
  *Example:*
  ```Lua
  settings = imppg.load_settings("/path/to/settings.xml")
  image = imppg.load_image("/path/to/image.tif")
  processed_image = imppg.process_image(image, settings)
  ```

- `new_settings`

  Creates a [settings](#settings) object which does not introduce any image changes when applied (i.e., disabled sharpening, identity tone curve). The processing steps can then be enabled selectively.

  *Parameters:* none

  ----
  *Example:*
  ```Lua
  settings = imppg.new_settings()
  settings:lr_deconv_sigma(1.3)
  ```

- `process_image_file`

  Processes an image file according to a settings file and saves the result.

  *Parameters:*
  - image file path
  - settings file path
  - processed image file path
  - output format (one of `imppg.BMP_8`, `imppg.PNG_8`, `imppg.TIFF_8_LZW`, `imppg.TIFF_16`, `imppg.TIFF_16_ZIP`, `imppg.TIFF_32F`, `imppg.TIFF_32F_ZIP`, `imppg.FITS_8`, `imppg.FITS_16`, `imppg.FITS_32F`)

  ----
  *Example*
  ```Lua
  imppg.process_image_file("/path/to/image.tif", "/path/to/settings.xml", "/path/to/output.png", imppg.PNG_8)
  ```

- `process_image`

  Processes image according to given settings and returns the result.

  *Parameters:*
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

- `load_image_split_rgb`

  Loads an RGB image and splits it into R, G, B channels. The resulting objects can be passed to image processing functions.

  *Parameters:*
  - image file path

  ----
  *Example*
  ```Lua
  r, g, b = imppg.load_image_split_rgb("/path/to/rgb_image.tif")
  settings = imppg.load_settings("/path/to/settings.xml")
  processed_r = imppg.process_image(r, settings)
  processed_g = imppg.process_image(g, settings)
  processed_b = imppg.process_image(b, settings)
  ```

- `combine_rgb`

  Combines 3 monochrome images representing R, G, B channels into an RGB image.

  *Parameters:*
  - image containing R channel
  - image containing G channel
  - image containing B channel

  ----
  *Example*
  ```Lua
  r, g, b = imppg.load_image_split_rgb("/path/to/rgb_image.tif")
  settings = imppg.load_settings("/path/to/settings.xml")
  processed_r = imppg.process_image(r, settings)
  processed_g = imppg.process_image(g, settings)
  processed_b = imppg.process_image(b, settings)
  rgb = imppg.combine_rgb(processed_r, processed_g, processed_b)
  ```

- `progress`

  Sets the value of the "Run script" dialog's progress bar.

  *Parameters:*
  - progress fraction (from 0.0 to 1.0)

  ----
  *Example*
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

- `print`

  Prints a message in the "Run script" dialog's console.

  *Parameters:*
  - message (string)

  ----
  *Example*
  ```Lua
  settings = imppg.load_settings("/path/to/settings.xml")
  files, num_files = imppg.filesystem.list_files_sorted("/path/to/stack*.tif")
  for index, file in ipairs(files) do
      imppg.print(string.format("Processing file %s.", file))
      image = imppg.load_image(file)
      processed_image = imppg.process_image(image, settings)
      processed_image:save(string.format("/path/to/processed/output_%02d.png", index), imppg.PNG_8)
      imppg.progress(index / num_files)
  end
  ```

- `align_images`

  Aligns an image sequence (e.g., to create a time-lapse animation).

  *Parameters:*
  - input file paths (table)
  - alignment mode: `imppg.STANDARD` (phase correlation) or `imppg.SOLAR_LIMB`
  - crop mode: `imppg.CROP` (crops to intersection) or `imppg.PAD` (pads to bounding box)
  - subpixel alignment (Boolean)
  - output directory
  - output file name suffix (can be `nil`)
  - progress callback (can be `nil`); a function taking a number (0 to 1) as a parameter; *note:* currently ignored,
    the alignment progress is reflected directly in the progress bar

  ----
  *Examples*

  ```Lua
  imppg.align_images(
      imppg.filesystem.list_files_sorted("/images/frame*.png"),
      imppg.STANDARD,
      imppg.CROP,
      true,
      "/images/aligned",
      nil,
      nil
  )
  ```


### Module `imppg.filesystem`

- `list_files`

  Returns an iterator over all files matching the file path mask. The iteration is performed in an unspecified order. *Note:* typically `list_files_sorted` is more convenient.

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

  Returns a sorted table (and its length) of files matching the file path mask (see `list_files` for examples of masks).

  *Parameters:*
  - file path mask (wildcards are supported)

  ----
  *Example:*
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

## Classes

### Class `image`

Methods:

- `align_rgb`

  Aligns R, G, B channels using phase correlation.

  *Parameters:* none

  ----
  *Example*
  ```Lua
  r, g, b = imppg.load_image_split_rgb("/path/to/rgb_image.tif")
  settings = imppg.load_settings("/path/to/settings.xml")
  processed_r = imppg.process_image(r, settings)
  processed_g = imppg.process_image(g, settings)
  processed_b = imppg.process_image(b, settings)
  rgb = imppg.combine_rgb(processed_r, processed_g, processed_b)
  rgb.align_rgb()
  rgb:save("/path/to/output.png", imppg.PNG_8)
  ```

- `awb`

  Performs automatic white balancing using the “gray world” method.

  *Parameters:*: none

  ----
  *Example*
  ```Lua
  image = imppg.load_image("/path/to/rgb_image.tif")
  image:awb()
  ```

- `multiply`
  Multiplies all pixel values by given factor.

  Can be useful when the raw stacks of a sequence are too bright (and sharpening them overflows the histogram), but normalization cannot be used (e.g., it introduces animation flicker).

  *Parameters:*
  - factor (non-negative)

  ----
  *Example*
  ```Lua
  image = imppg.load_image("/path/to/image.tif")
  image:multiply(0.8)
  ```

- `save`
  Saves image to file.

  *Parameters:*
  - file path
  - output format (one of `imppg.BMP_8`, `imppg.PNG_8`, `imppg.TIFF_8_LZW`, `imppg.TIFF_16`, `imppg.TIFF_16_ZIP`, `imppg.TIFF_32F`, `imppg.TIFF_32F_ZIP`, `imppg.FITS_8`, `imppg.FITS_16`, `imppg.FITS_32F`)

  ----
  *Example*
  ```Lua
  settings = imppg.load_settings("/path/to/settings.xml")
  image = imppg.load_image("/path/to/image.tif")
  processed_image = imppg.process_image(image, settings)
  processed_image:save("/path/to/output.png", imppg.PNG_8)
  ```

### Class `settings`

Methods:

- `get_normalization_enabled`

  Returns whether image brightness normalization is enabled.

  *Parameters:* none

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  print(s:get_normalization_enabled())
  ```

- `normalization_enabled`

  Sets whether image brightness normalization is enabled.

  *Parameters:*
  - enabled flag

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  s.normalization_enabled(true)
  ```

- `get_normalization_min`

  Returns the minimum brightness level after normalization.

  *Parameters:* none

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  print(s:get_normalization_min())
  ```

- `normalization_min`

  Sets the minimum brightness level after normalization.

  *Parameters:*
  - value from 0.0 to 1.0

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  s:normalization_min(0.1)
  ```

- `get_normalization_max`

  Returns the maximum brightness level after normalization.

  *Parameters:* none

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  print(s:get_normalization_max())
  ```

- `normalization_max`

  Sets the maximum brightness level after normalization.

  *Parameters:*
  - value from 0.0 to 1.0

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  s:normalization_max(0.8)
  ```

- `get_lr_deconv_sigma`

  Returns Lucy-Richardson deconvolution *sigma*.

  *Parameters:* none

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  print(s:get_lr_deconv_sigma())
  ```

- `lr_deconv_sigma`

  Sets Lucy-Richardson deconvolution *sigma*.

  *Parameters:*
  - L-R sigma

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  s:lr_deconv_sigma(1.3)
  ```

- `get_lr_deconv_num_iters`

  Returns Lucy-Richardson deconvolution iteration count.

  *Parameters:* none

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  print(s:get_lr_deconv_num_iters())
  ```

- `lr_deconv_num_iters`

  Sets Lucy-Richardson deconvolution iteration count.

  *Parameters:*
  - L-R iteration count

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  ```

- `get_lr_deconv_deringing`

  Returns whether deringing is enabled for L-R deconvolution.

  *Parameters:* none

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  print(s:get_lr_deconv_deringing())
  ```

- `lr_deconv_deringing`

  Sets whether deringing is enabled for L-R deconvolution. Note that deringing only takes effect around totally white (overexposed) areas.

  *Parameters:*
  - enabled flag

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  s:lr_deconv_deringing(true)
  ```

- `get_unsh_mask_adaptive`

  Returns whether adaptive unsharp masking is enabled.

  *Parameters:*
  - mask index (0-based)

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  print(s:get_unsh_mask_adaptive(0))
  ```

- `unsh_mask_adaptive`

  Sets whether adaptive unsharp masking is enabled.

  *Parameters:*
  - mask index (0-based)
  - enabled flag

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  s:unsh_mask_adaptive(0, true)
  ```

- `get_unsh_mask_sigma`

  Returns unsharp mask *sigma*.

  *Parameters:*
  - mask index (0-based)

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  print(s:get_unsh_mask_sigma(0))
  ```

- `unsh_mask_sigma`

  Sets unsharp mask *sigma*.

  *Parameters:*
  - mask index (0-based)
  - sigma

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  s:unsh_mask_sigma(0, 1.4)
  ```

- `get_unsh_mask_amount_min`

  Gets min. amount of unsharp masking (effective only if adaptive unsharp masking is enabled).

  *Parameters:*
  - mask index (0-based)

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  s:get_unsh_mask_amount_min(0)
  ```

- `unsh_mask_amount_min`

  Sets min. amount of unsharp masking (effective only if adaptive unsharp masking is enabled).

  *Parameters:*
  - mask index (0-based)
  - min. amount

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  s:unsh_mask_amount_min(0, 2.3)
  ```

- `get_unsh_mask_amount_max`

  Gets max. amount of unsharp masking (same as `get_unsh_mask_amount`).

  *Parameters:*
  - mask index (0-based)

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  print(s:get_unsh_mask_amount_max(0))
  ```

- `unsh_mask_amount_max`

  Sets max. amount of unsharp masking (same as `unsh_mask_amount`).

  *Parameters:*
  - mask index (0-based)
  - amount max.

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  s:unsh_mask_amount_max(0, 5.5)
  ```

- `get_unsh_mask_amount`

  Returns unsharp masking amount (same as `get_unsh_mask_amount_max`).

  *Parameters:*
  - mask index (0-based)

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  print(s:get_unsh_mask_amount(0))
  ```

- `unsh_mask_amount`

  Sets unsharp masking amount (same as `unsh_mask_amount_max`).

  *Parameters:*
  - mask index (0-based)
  - amount

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  s:unsh_mask_amount(0, 5.5)
  ```

- `get_unsh_mask_threshold`

  Returns adaptive unsharp masking brightness threshold (effective only if adaptive mode is enabled).

  *Parameters:*
  - mask index (0-based)

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  print(s:get_unsh_mask_threshold(0))
  ```

- `unsh_mask_threshold`

  Sets adaptive unsharp masking brightness threshold (effective only if adaptive mode is enabled).

  *Parameters:*
  - mask index (0-based)
  - threshold

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  s:unsh_mask_threshold(0, 0.1)
  ```

- `get_unsh_mask_twidth`

  Returns adaptive unsharp masking transition width (effective only if adaptive mode is enabled).

  *Parameters:*
  - mask index (0-based)

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  print(s:get_unsh_mask_twidth(0))
  ```

- `unsh_mask_twidth`

  Sets adaptive unsharp masking transition width (effective only if adaptive mode is enabled).

  *Parameters:*
  - mask index (0-based)
  - transition width

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  s:unsh_mask_twidth(0, 0.15)
  ```

- `tc_add_point`

  Adds a tone curve point.

  *Parameters:*
  - X coordinate (value between 0.0 and 1.0); must be different than existing points
  - Y coordinate (value between 0.0 and 1.0)

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  ```

- `tc_set_point`

  Sets coordinates of tone curve point.

  *Parameters:*
  - point index (0-based)
  - X coordinate (value > X of the left neighbor and < X of the right neighbor)
  - Y coordinate (value between 0.0 and 1.0)

  ----
  *Example*
  ```Lua
  s = imppg.new_settings()
  ```
