// Tests a global variable initialized to the result of a complex expression
// involving multiple other globals. Exercises the global initializer path.
//
// Expected stdout:
//   puti(final, 4) -> prints 1250   (50 * 25 = 1250)
// good

width: int = 50
height: int = width / 2
area: int = width * height

puti(area, 4)
