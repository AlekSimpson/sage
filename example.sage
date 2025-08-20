

// should throw an identifier resolution error, no circular dependencies

foo :: (something: int) {
    a := 4 +4 
    bar(a)
}


bar :: (something: int) {
    foo(b)
    b := 34 + a
    a := 3
}
