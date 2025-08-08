
calc :: (c int) -> int {
    ret c + 2
}

main :: () {

    E: int = 23

    C: int = B + A // depends on B and A
    B: int = A * 2 // depends on A
    A: int = 10 // depends on nothing
    D: int = calc(C) // depends on calc and C

    F: int = 34 + E
}
