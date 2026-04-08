
array_add :: (x: int[3], y: int[3]) -> int {
    ret x[0] + y[0]
}

array: int[3] = [1, 2, 3]

array_add(2, array)