print("Lua script starts.")

-- for i = 0, 5, 1 do
--     print("waiting: ", i)
--     os.execute("sleep 1")
-- end

imppg.handler1()

object1 = imppg.create_dummy1()
print("object1 returns: ", object1:get())

object2 = imppg.create_dummy2(44)
print("object2 returns: ", object2:get(33))

imppg.filesystem.dummy()
imppg.dummy2.dummy()
imppg.filesystem.subdummy.dummy()

for f in imppg.filesystem.list_files("/home/filip/Pictures/temp/tempj/sel/sm/*.zip") do
    print("listed file: ", f)
end


-- TESTING ###############

s = imppg.load_settings("my_settings.xml")
img = imppg.load_image("my_image.tif")
img = imppg.process(img, s)

s = imppg.new_settings()
s:unsh_mask_sigma(1.3)
s:unsh_mask_amount(2.4)
s:unsh_mask(1.3, 2.4)

tc = s:get_tone_curve()
s:set_tone_curve(tc)
tc = imppg.new_tone_curve()
tc = imppg.new_tone_curve(0.0, 0.0, 1.0, 1.0)
(x, y) = tc:get_point(0)
tc:set_point(5, 0.5, 0.5)

-- END TESTING ############

print("Lua script ends.")
