// Tests a struct returned from a constructor function where the caller then
// accesses fields immediately on the returned value (no extra temp needed).
//
// Expected stdout:
//   puti(global_v.x, 1) -> prints 7
//   puti(global_v.y, 1) -> prints 8
// good

Vec2 :: struct {
    x: i64
    y: i64
}

make_vec :: (x: i64, y: i64) -> Vec2 {
    v: Vec2
    v.x = x
    v.y = y
    ret v
}

global_v: Vec2 = make_vec(7, 8)
puti(global_v.x, 1)
puti(global_v.y, 1)
