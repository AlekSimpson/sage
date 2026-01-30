
add :: (x: int, y: int) -> int {
    ret x + y
}

main :: () {

    #run {
        puts("at comptime\n", 12)
    }


    result: int = add(5, 10)
    puts("tests\n", 6)

    puts("other\n", 6)
    puti(result, 2)


}



