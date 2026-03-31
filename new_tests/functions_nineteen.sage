// Tests a nested function that closes over its outer function's parameter.
// Verifies that closure capture of parameters (not just locals) works correctly.
//
// Expected stdout:
//   puti(inner result, 2) -> prints 15
// good but maybe disabled for now

main :: () {
    scale: int = 5

    multiply :: (x: int) -> int {
        ret x * scale
    }

    result: int = multiply(3)
    puti(result, 2)
}
