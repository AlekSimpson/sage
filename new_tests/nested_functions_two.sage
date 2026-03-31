// Tests that a nested function can be called multiple times and that
// the closure variables it reads remain correct across calls.
//
// Expected stdout (called twice):
//   puti(val, 2) -> prints 50
//   puti(val, 2) -> prints 50
// good

main :: () {
    multiplier: int = 10

    scale :: (x: int) -> int {
        ret x * multiplier
    }

    puti(scale(5), 2)
    puti(scale(5), 2)
}
