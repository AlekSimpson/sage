// Tests two different struct types used together in the same scope.
// Verifies that type-specific field offsets don't bleed between struct types.
//
// Expected stdout:
//   puti(p.x + p.y, 2)       -> prints 30
//   puti(d.width * d.height, 4) -> prints 200
// good

Point :: struct {
    x: int
    y: int
}

Rect :: struct {
    width: int
    height: int
}

p: Point
p.x = 10
p.y = 20

d: Rect
d.width = 10
d.height = 20

puti(p.x + p.y, 2)
puti(d.width * d.height, 3)
