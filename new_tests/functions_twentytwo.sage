// Tests a function with three parameters (between the 2-param and 4-param cases).
// Verifies that argument ordering is preserved for odd-numbered parameter counts.
//
// Expected stdout:
//   puti(result, 2) -> prints 6  (1 * 2 * 3)
// good

multiply_three :: (a: int, b: int, c: int) -> int {
    ret a * b * c
}

result: int = multiply_three(1, 2, 3)
puti(result, 1)
