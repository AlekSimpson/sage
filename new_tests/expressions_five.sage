// Tests integer division truncation and subtraction ordering.
// Verifies that division truncates toward zero and subtraction is left-associative.
//
// Expected stdout:
//   puti(a, 1) -> prints 3   (7 / 2 = 3, truncated)
//   puti(b, 1) -> prints 4   (10 - 3 - 3 = 4)
// good

a: int = 7 / 2
b: int = 10 - 3 - 3

puti(a, 1)
puti(b, 1)
