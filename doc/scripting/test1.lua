for i = 1, 10, 1 do
    t0 = os.clock()
    while os.clock() - t0 < 1 do end

    imppg.progress(i / 10)
end
