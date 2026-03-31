// Tests writing to an int through a pointer and verifying the original
// variable reflects the updated value. Distinguishes from pointer_deref_one
// which only reads through the pointer.
//
// Expected stdout:
//   puti(x, 1) -> prints 5   (before)
//   puti(x, 1) -> prints 99  (after write through ptr)
// good

x: int = 5
ptr: int* = ^x

puti(x, 1)

@ptr = 99

puti(x, 2)
