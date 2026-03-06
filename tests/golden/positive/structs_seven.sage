Vector :: struct {
    x: int
    y: int
}

vec: Vector
foo: Vector* = ^vec

vec.x = 45
vec.y = 23

puti(foo.x, 2)
puti(foo.y, 2)
