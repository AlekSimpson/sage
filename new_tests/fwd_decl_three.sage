// Tests a struct defined after the variable that uses it — forward struct
// declaration resolution. Mirrors the pattern in structs_five.
//
// Expected stdout:
//   puti(val.count, 2) -> prints 7
// good

val: Counter
val.count = 7

Counter :: struct {
    count: int
}

puti(val.count, 1)
