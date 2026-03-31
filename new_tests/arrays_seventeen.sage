// Tests an i32 array (different element type from most tests which use i64/int).
// Verifies that the element size used in address calculations is correct for i32.
//
// Expected stdout:
//   puti(vals[0], 1) -> prints 100
//   puti(vals[1], 1) -> prints 200
//   puti(vals[2], 1) -> prints 300
// good

vals: i32[3] = [100, 200, 300]
puti(vals[0], 3)
puti(vals[1], 3)
puti(vals[2], 3)
