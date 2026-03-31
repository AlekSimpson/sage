// Integration: global array of structs populated via a constructor function.
// Combines arrays_eight-style struct arrays with functions_eight-style constructors.
//
// Expected stdout:
//   puti(vecs[0].x, 1) -> prints 1
//   puti(vecs[0].y, 1) -> prints 2
//   puti(vecs[1].x, 1) -> prints 3
//   puti(vecs[1].y, 1) -> prints 4
// good

Vec :: struct {
    x: i64
    y: i64
}

make_vec :: (x: i64, y: i64) -> Vec {
    v: Vec
    v.x = x
    v.y = y
    ret v
}

vecs: Vec[2]
vecs[0] = make_vec(1, 2)
vecs[1] = make_vec(3, 4)

puti(vecs[0].x, 1)
puti(vecs[0].y, 1)
puti(vecs[1].x, 1)
puti(vecs[1].y, 1)
