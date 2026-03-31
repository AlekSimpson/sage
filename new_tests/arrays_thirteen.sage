// Tests passing an array element directly as an argument to a user-defined function.
// Verifies that indexed loads are correctly materialized at call sites.
//
// Expected stdout:
//   puti(result, 2) -> prints 10
// good

double :: (x: int) -> int {
    ret x * 2
}

vals: int[3] = [3, 5, 7]
result: int = double(vals[1])
puti(result, 2)
