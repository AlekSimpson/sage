#run {
    executable_name = "output"
    platform = "LINUX"
    architecture = "X86"
    bitsize = 64
}

main :: () -> void {
    result i64 = 2 + 2 * 10
    printf("this result is: %d\n", result)
}

