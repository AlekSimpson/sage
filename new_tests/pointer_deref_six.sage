// Tests reassigning a pointer to point to a different variable.
// Verifies that after reassignment the pointer tracks the new target.
//
// Expected stdout:
//   puti(@ptr, 1) -> prints 10   (pointing at a)
//   puti(@ptr, 1) -> prints 20   (now pointing at b)
// good

a: int = 10
b: int = 20

ptr: int* = ^a
puti(@ptr, 2)

ptr = ^b
puti(@ptr, 2)
