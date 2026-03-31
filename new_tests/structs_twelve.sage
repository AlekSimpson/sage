// Tests passing a struct field directly as an argument to a function.
// Verifies that field loads are materialized correctly at call sites.
//
// Expected stdout:
//   puti(add(v.x, v.y), 2) -> prints 30
// good

add :: (a: int, b: int) -> int {
    ret a + b
}

Vec :: struct {
    x: int
    y: int
}

v: Vec
v.x = 10
v.y = 20

result: int = add(v.x, v.y)
puti(result, 2)
