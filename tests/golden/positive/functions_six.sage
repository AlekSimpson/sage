SOME_CONSTANT: int = 56

foo :: () {
    a: int = 3
    b: int = 1000
    foo_x: int = 40 + SOME_CONSTANT
}

main :: () {

    foo()
    foo()
}