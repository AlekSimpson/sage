// Tests a struct with four fields to verify field offset calculations
// remain correct as struct size grows beyond two fields.
//
// Expected stdout:
//   puti(r.x, 2) -> prints 1
//   puti(r.y, 2) -> prints 2
//   puti(r.z, 2) -> prints 3
//   puti(r.w, 2) -> prints 4
// good

Quad :: struct {
    x: int
    y: int
    z: int
    w: int
}

r: Quad
r.x = 1
r.y = 2
r.z = 3
r.w = 4

puti(r.x, 2)
puti(r.y, 2)
puti(r.z, 2)
puti(r.w, 2)
