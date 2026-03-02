

SOME_CONSTANT: int = 56

main :: () {
    offset: int = 39

    foo :: () {
        a: int = 3
        b: int = 1000
        foo_x: int = 40 + SOME_CONSTANT

        bar()
        bar :: () {
            bar_value: int = foo_x + offset
            offset = offset + 10
            puti(bar_value, 3)
        }

    }


    foo()
    foo()
}




