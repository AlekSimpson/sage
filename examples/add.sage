include "io"

#run {
    build_settings.executable_name = "output"
    build_settings.platform = "LINUX"
    build_settings.architecture = "X86"
    build_settings.bitsize = 64
}


main :: () -> void {
    result i32 = 2 + 2
    printf("%d\n", result)
    printf("Hello world\n")
}
