// Tests using a function return value directly in an expression without
// storing it in a temporary variable first.
//
// Expected stdout:
//   puti(add(3,4) * 2, 2) -> prints 14
// good

add :: (x: int, y: int) -> int {
    ret x + y
}

puti(add(3, 4) * 2, 2)
