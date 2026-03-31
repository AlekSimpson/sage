// Tests a struct whose fields are used inside an arithmetic expression,
// including overwriting a field with a computed value.
//
// Expected stdout:
//   puti(v.x, 2) -> prints 40
//   puti(v.y, 2) -> prints 100
// good

Vector2 :: struct {
    x: int
    y: int
}

v: Vector2
v.x = 10
v.y = 5

v.x = v.x * 4
v.y = v.x * v.y / 2

puti(v.x, 2)
puti(v.y, 3)
