// Register pressure: deeply chained arithmetic where every intermediate value
// must stay live until used. Forces the allocator to keep many values
// in registers simultaneously.
//
// Expected stdout:
//   result = (1+2)*(3+4) - (5+6)*(7-4) = 3*7 - 11*3 = 21 - 33 = -12
//   puti(result, 2) -> prints -12
// good

a: int = 1 + 2
b: int = 3 + 4
c: int = 5 + 6
d: int = 7 - 4

result: int = a * b - c * d
puti(result, 2)
