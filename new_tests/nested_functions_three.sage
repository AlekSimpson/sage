// Tests two sibling nested functions in the same outer scope, each using
// a different outer variable. Verifies that sibling closures don't share state.
//
// Expected stdout:
//   puti(get_a(), 1) -> prints 10
//   puti(get_b(), 1) -> prints 20
// good

main :: () {
    a: int = 10
    b: int = 20

    get_a :: () -> int {
        ret a
    }

    get_b :: () -> int {
        ret b
    }

    puti(get_a(), 2)
    puti(get_b(), 2)
}
