// Tests a three-level nested function call chain: outer defines middle,
// middle defines inner, inner reads a variable from outer's scope.
// Stresses closure variable capture across multiple nesting levels.
//
// Expected stdout:
//   puti(bar_value, 3) -> prints 103  (base=100, increment=3, total=103)
// good, but maybe need to disable

main :: () {
    base: int = 100

    outer :: () {
        increment: int = 3

        inner :: () {
            bar_value: int = base + increment
            puti(bar_value, 3)
        }

        inner()
    }

    outer()
}
