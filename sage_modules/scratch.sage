

#run {
    executable_name := "test"
    platform := "LINUX"
    architecture := "X86"
    bitsize := 64
}

struct string {
    content: char*
    length: i64
}

create_string :: (content: char*) -> string {
    iter: char
    length := 0
    while (iter != '\0') {
        iter = content[length]
        length++
    }

    ret string(content, length)
}

substring :: (using str: string, substr: string) -> int {
    cmp_index := 0
    starting_index := 0

    for content {
        if cmp_index == substr.length  break

        if it == substr.content[cmp_index] {
            // matching character
            cmp_index++

            if starting_index == 0 {
                starting_index == it
            }

            continue
        }

        cmp_index = 0
        starting_index = 0
    }

    if cmp_index < substr.length {
        // full substring should match not part of it
        ret -1
    }

    ret cmp_index
}

main :: () {
    printf("hello world!\n")

    #run {
        amount: int = 5
        strings: string[amount]
    }

    for 0..amount {
        strings[it] = create_string("hello test")
    }

    if strings[0].substring(strings[1]) != -1 {

    }
}

visit -> expression | statement
statement -> funcdef, struct, if, while, for, vardec | expression
expression -> varref, literal, funccall, binop, unop














