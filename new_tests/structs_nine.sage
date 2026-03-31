// Tests multiple independent instances of the same struct type in the same scope.
// Verifies that field writes to one instance do not corrupt another.
//
// Expected stdout:
//   puti(a.x + b.x, 2) -> prints 40
//   puti(a.y + b.y, 2) -> prints 60
// good

Point :: struct {
    x: int
    y: int
}

a: Point
b: Point

a.x = 10
a.y = 20
b.x = 30
b.y = 40

puti(a.x + b.x, 2)
puti(a.y + b.y, 2)
