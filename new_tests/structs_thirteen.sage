// Tests a struct containing a bool field alongside int fields.
// Verifies that mixed-type structs compute field offsets correctly.
//
// Expected stdout:
//   puti(s.count, 2) -> prints 7
// good

State :: struct {
    active: bool
    count: int
}

s: State
s.active = true
s.count = 7

puti(s.count, 1)
