counter = 0
for file in imppg.filesystem.list_files("/home/filip/Pictures/astrofoto/2022-08-30 Flüela Pass/raw/l1*.tif") do
    imppg.process_image_file(
        file,
        "/home/filip/Pictures/astrofoto/2022-08-30 Flüela Pass/lum.xml",
        string.format("/home/filip/Documents/temp/%04d_out.png", counter),
        imppg.PNG_8
    )
    -- imppg.progress(counter)
    counter = counter + 1
end
