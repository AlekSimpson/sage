

Scalar :: struct {
    value: int
}

offset: Scalar
offset.value = 4

main :: () {

    foo :: () {
        a: int = 3
        b: int = 1000
        foo_x: int = 40

        bar()
        bar :: () {
            bar_value: int = foo_x + offset.value
            offset.value = offset.value + 10
            puti(bar_value, 2)
        }

    }


    foo()
    foo()
}




