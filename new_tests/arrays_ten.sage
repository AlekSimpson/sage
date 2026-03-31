// Tests an array returned from a function where the caller then accesses
// both indexed elements and the .length property on the returned array.
//
// Expected stdout:
//   puti(result[0], 1) -> prints 4
//   puti(result[1], 1) -> prints 5
//   puti(result[2], 1) -> prints 6
//   puti(result.length, 1) -> prints 3
// good

make_array :: () -> int[3] {
    ret [4, 5, 6]
}

result: int[3] = make_array()
puti(result[0], 1)
puti(result[1], 1)
puti(result[2], 1)
puti(result.length, 1)
