// Tests two pointers pointing to two different variables simultaneously.
// Verifies that dereferencing each pointer yields its own variable's value
// and that the pointers don't alias each other.
//
// Expected stdout:
//   puti(@pa, 1) -> prints 10
//   puti(@pb, 1) -> prints 20
// good

a: int = 10
b: int = 20

pa: int* = ^a
pb: int* = ^b

puti(@pa, 2)
puti(@pb, 2)
