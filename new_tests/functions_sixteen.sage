// Tests calling the same function multiple times with different arguments.
// Verifies that the call frame is correctly reset between invocations.
//
// Expected stdout:
//   puti(r1, 2) -> prints 3
//   puti(r2, 2) -> prints 10
//   puti(r3, 2) -> prints 21
// good

add :: (x: int, y: int) -> int {
    ret x + y
}

r1: int = add(1, 2)
r2: int = add(3, 7)
r3: int = add(10, 11)

puti(r1, 1)
puti(r2, 2)
puti(r3, 2)
