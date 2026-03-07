
// every struct created automatically has a default constructor function created that just has all the members as parameters in a function name after the type
string :: struct {
    content: char*, 
    length: int, 
    start: char,
    end: char
}

// interpolate :: (using str: string, args: ...void*) -> void {
// }

interpolate :: (str: string*, args: ...void*) -> void {
    // TODO: need to figure out casting types
}

printf :: (input: string, args: ...void*) -> void {
    puts :: (seq: char*, len: int) -> void {
        // sys_write, stdout, pointer to string, number of bytes
        syscall(1, 1, &input, len*8)
    }

    // input.interpolate(args)
    interpolate(&input, args)

    puts(input.content, input.length)
}

