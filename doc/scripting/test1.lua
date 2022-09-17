counter = 0
for file in imppg.filesystem.list_files("/home/filip/Pictures/astrofoto/2022-08-30 Flüela Pass/raw/l1*.tif") do
    imppg.process_image_file(
        file,
        "/home/filip/Pictures/astrofoto/2022-08-30 Flüela Pass/lum.xml", string.format("%04d_out.png", counter),
        imppg.PNG_8
    )
    counter = counter + 1
end
