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



/////

"executable_name" -> {53453, "executable_name", 2, 5, ...}
"platform" -> {5462341, "platform", 3, 5, ...}
"architecture" -> {982304, "architecture", 4, 5, ...}
"bitsize" -> {345234, "bitsize", 5, 5, ...}

"result" -> {245234, "result", 10, 11, ...}
