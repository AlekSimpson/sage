// Tests register allocator under pressure: many independent local variables
// live simultaneously in one function scope. If the allocator spills or
// incorrectly reuses registers the sum will be wrong.
//
// Expected stdout:
//   puti(sum, 2) -> prints 36
// good

main :: () {
    a: int = 1
    b: int = 2
    c: int = 3
    d: int = 4
    e: int = 5
    f: int = 6
    g: int = 7
    h: int = 8

    sum: int = a + b + c + d + e + f + g + h
    puti(sum, 2)
}
