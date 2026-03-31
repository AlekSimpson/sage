// Tests a function calling another function internally.
// cube() depends on square(), verifying inter-function call correctness.
//
// Expected stdout:
//   puti(result, 2) -> prints 27
// good

square :: (x: int) -> int {
    ret x * x
}

cube :: (x: int) -> int {
    ret x * square(x)
}

result: int = cube(3)
puti(result, 2)
