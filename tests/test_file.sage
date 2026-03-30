

make_string :: () -> string {
    local_test: string = "it works?"
    ret local_test
}

factory_string : string = make_string()
puts(factory_string, factory_string.length)


