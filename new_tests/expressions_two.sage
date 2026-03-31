// Tests global constants (declared with ::) referenced inside a function body
// as part of an arithmetic expression. Verifies constant propagation into
// function scope.
//
// Expected stdout:
//   puti(result, 3) -> prints 115
// good

BASE: int = 100
MULTIPLIER: int = 3

compute :: (x: int) -> int {
    ret BASE + x * MULTIPLIER
}

result: int = compute(5)
puti(result, 3)
