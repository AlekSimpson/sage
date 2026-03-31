// Tests a function with 4 parameters to stress the calling convention
// and register allocation for argument passing.
//
// Expected stdout:
//   puti(result, 2) -> prints 10
// good

add_four :: (a: int, b: int, c: int, d: int) -> int {
    ret a + b + c + d
}

result: int = add_four(1, 2, 3, 4)
puti(result, 2)
