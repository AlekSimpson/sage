// Tests a global struct that is populated at global scope (not inside main),
// mirroring the pattern used throughout the struct tests.
// Verifies struct fields survive the global initializer phase.
//
// Expected stdout:
//   puti(origin.x, 2) -> prints 0
//   puti(origin.y, 2) -> prints 0
//   puti(offset.x, 2) -> prints 5
//   puti(offset.y, 2) -> prints 10
// good

Vec :: struct {
    x: int
    y: int
}

origin: Vec
origin.x = 0
origin.y = 0

offset: Vec
offset.x = 5
offset.y = 10

puti(origin.x, 1)
puti(origin.y, 1)
puti(offset.x, 1)
puti(offset.y, 2)
