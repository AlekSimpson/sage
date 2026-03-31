// Tests forward-declared globals used in a function body — the function is
// defined before the globals it references are declared.
//
// Expected stdout:
//   puti(get_total(), 2) -> prints 30
// good

get_total :: () -> int {
    ret X + Y
}

X: int = 10
Y: int = 20

puti(get_total(), 2)
