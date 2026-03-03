

Vector2 :: struct {
    x: int
    y: int
}

some_vector: Vector2
some_vector.x = 5
some_vector.y = 10

some_vector.y = some_vector.y * 2
some_vector.x = some_vector.y + 20

puti(some_vector.x, 2)

