// Integration: pointer to a struct, function that takes an int and returns
// a modified copy, combined with pointer dereference to supply the argument.
//
// Expected stdout:
//   puti(result, 2) -> prints 20   (val.x=10, doubled=20)
// good

double :: (x: int) -> int {
    ret x * 2
}

Box :: struct {
    x: int
}

val: Box
val.x = 10

ptr: Box* = ^val
result: int = double(ptr.x)
puti(result, 2)
