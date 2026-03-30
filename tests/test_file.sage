
Vector :: struct {
    x: i64
    y: i64
}

make_vector :: (x: i64, y: i64) -> Vector {
    return_value: Vector
    return_value.x = x
    return_value.y = y
    ret return_value
}

global_vector: Vector = make_vector(5, 6)
puti(global_vector.x, 1)
puti(global_vector.y, 1)



