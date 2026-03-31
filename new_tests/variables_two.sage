// Tests a chain of global variable declarations where each depends on
// previously declared globals. Stresses forward declaration resolution
// across multiple levels of dependency.
//
// Expected stdout:
//   puti(A, 2) -> prints 10
//   puti(B, 2) -> prints 20
//   puti(C, 2) -> prints 30
// good

A: int = 10
B: int = A * 2
C: int = A + B

puti(A, 2)
puti(B, 2)
puti(C, 2)
