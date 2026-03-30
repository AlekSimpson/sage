

make_array :: () -> int[3] {
    ret [4, 5, 6]
}

factory_array : int[3] = make_array()
puti(factory_array[0], 1)
puti(factory_array[1], 1)
puti(factory_array[2], 1)


