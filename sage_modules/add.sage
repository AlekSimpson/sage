
add :: (x: int, y: int) -> int {
    ret x + y
}

main :: () {

    #run {
        puts("at comptime, hello world, this string is really long!\n", 54)
    }


    result: int = add(5, 10)
    puts("tests\n", 6)

    puts("other\n", 6)
    puti(result, 2)


}



