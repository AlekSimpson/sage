

Vector2 :: struct {
    x: i64
    y: i64
}

SomeStruct :: struct {
    vector1: Vector2*
}

some_value: SomeStruct
some_value.vector1 = ^vector

vector: Vector2
vector.y = 1034

puti(some_value.vector1.y, 4)


