// Tests reassigning a string variable and verifying .length reflects the new value.
//
// Expected stdout:
//   puti(s.length, 1)  -> prints 2  ("hi")
//   puti(s.length, 1)  -> prints 5  ("world")
// good

s: string = "hi"
puti(s.length, 1)

s = "world"
puti(s.length, 1)
