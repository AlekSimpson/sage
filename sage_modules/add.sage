#run { // proc 1
    executable_name = "output"
    platform = "LINUX"
    architecture = "X86"
    bitsize = 64
}

FIBONACCI_10 :: #run { // proc 2
    a := 0
    b := 1
    for 1..10 {
        temp := a + b
        a = b
        b = temp
    }
    ret b
}

main :: () -> void {
    sub :: (a: int, b: int) -> int { // proc 5
        ret a - b
    }

    #run { // proc 3
        bar: i32
        #run { // proc 4
            bar = 34
        }

        foo := bar + 2 + (sub(49, 29) + 4)
    }

    result: i64 = 2 + 2 * 10 + FIBONACCI_10
    printf("this result is: %d\n", result)
}

