// Tests an expression that mixes a struct field, a global constant, and a local
// variable in a single arithmetic expression.
//
// Expected stdout:
//   result = v.x + OFFSET - local_adj = 10 + 100 - 5 = 105
//   puti(result, 3) -> prints 105
// good

OFFSET: int = 100

Vec :: struct {
    x: int
}

v: Vec
v.x = 10

local_adj: int = 5
result: int = v.x + OFFSET - local_adj
puti(result, 3)
