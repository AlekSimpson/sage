// Tests that multiple user-defined functions can all be called from inside main,
// and that their return values are independently correct.
//
// Expected stdout:
//   puti(a, 1) -> prints 3
//   puti(b, 1) -> prints 5
//   puti(c, 1) -> prints 7
// good

inc :: (x: int) -> int {
    ret x + 1
}

dec :: (x: int) -> int {
    ret x - 1
}

main :: () {
    a: int = inc(2)
    b: int = inc(4)
    c: int = dec(8)
    puti(a, 1)
    puti(b, 1)
    puti(c, 1)
}
