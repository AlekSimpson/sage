// Tests forward declaration where a global variable depends on another global
// that depends on a global constant — a three-step dependency chain.
//
// Expected stdout:
//   puti(C, 2) -> prints 24  (BASE=6, B=BASE*2=12, C=B*2=24)
// good

B: int = BASE * 2
C: int = B * 2
BASE: int = 6

puti(C, 2)
