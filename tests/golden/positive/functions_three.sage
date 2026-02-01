


main:: () {
    adder:: (x: int, y: int) -> int {
        ret x + y
    }

    fsub:: (x: f64, y: f64) -> f64 {
        ret x - y
    }

    foo :: () {
        bar:: () {
            puts("bar", 3)
        }

        puts("foo", 3)
        bar()
        bar()
        bar()
    }


    foo()

    flt: f64 = fsub(30.1, 123.34)
    value: int = adder(34, 34)


}





