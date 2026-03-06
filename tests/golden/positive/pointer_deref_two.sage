

Vector :: struct {
    x: int*
    y: int*
}

a: int
b: int
vector: Vector

a = 5
b = 6

vector.x = ^a
vector.y = ^b

vecx_copy: int = @vector.x
vecy_copy: int = @vector.y

puti(vecx_copy, 1)
puti(vecy_copy, 1)

