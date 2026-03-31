// Tests a deeply nested arithmetic expression to stress operator precedence
// and the expression tree code generation path.
//
// Expected stdout:
//   result = ((2 + 3) * (4 - 1)) / (5 - 2) = (5 * 3) / 3 = 5
//   puti(result, 1) -> prints 5
// good

result: int = ((2 + 3) * (4 - 1)) / (5 - 2)
puti(result, 1)
