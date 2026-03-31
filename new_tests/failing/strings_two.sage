// Tests multiple string variables declared in the same scope and their
// .length properties used individually and in arithmetic.
//
// Expected stdout:
//   puti(s1.length, 1)       -> prints 5  ("hello")
//   puti(s2.length, 1)       -> prints 4  ("sage")
//   puti(s1.length + s2.length, 1) -> prints 9
// good

s1: string = "hello"
s2: string = "sage"

puti(s1.length, 1)
puti(s2.length, 1)
puti(s1.length + s2.length, 1)
