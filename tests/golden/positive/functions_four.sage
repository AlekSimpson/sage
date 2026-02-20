

SOME_CONSTANT: int = 56

main :: () {
    foo :: () {
        foo_x: int = 40 + SOME_CONSTANT

        bar :: () {
            puti(foo_x, 2)
        }
    }


    foo()
    foo()
}




