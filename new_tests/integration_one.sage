// Integration: a function returns a struct and the caller immediately reads
// fields from it — no explicit temporary struct variable needed on the call site.
//
// Expected stdout:
//   puti(pair.a, 2) -> prints 7
//   puti(pair.b, 2) -> prints 13
// good

Pair :: struct {
    a: i64
    b: i64
}

make_pair :: (x: i64, y: i64) -> Pair {
    p: Pair
    p.a = x
    p.b = y
    ret p
}

pair: Pair = make_pair(7, 13)
puti(pair.a, 2)
puti(pair.b, 2)
