for i = 1, 10, 1 do
    t0 = os.clock()
    while os.clock() - t0 < 1 do end

    print("sending fraction of ", i / 10)
    imppg.progress(i / 10)
end
