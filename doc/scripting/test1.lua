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





print("Lua script ends.")
