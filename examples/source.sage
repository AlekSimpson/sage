
Car :: struct {



    door_count int,



    plate_num int,

}

sub :: (a int, b int) -> int {
    ret a - b
}

main :: () -> void {
    count int
    pointer_test int*
    arraytest [[char]]

    for i in 5...10 {
        printf(i)
    }

    if count > 10 {
        count = count + 4
    }else if count < 6 {
        count = count + 50
        ret count
    }else {
        ret count
    }

    another_var int
    while 10 > another_var {
        another_var = another_var - 1
    }
}


add :: (a int, b int) -> int {
    ret a + b
}
