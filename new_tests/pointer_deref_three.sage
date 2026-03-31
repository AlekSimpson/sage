// Tests taking the address of a struct field directly (^struct.field),
// then reading back the value through the pointer.
// Verifies that address-of on a field offset is computed correctly.
//
// Expected stdout:
//   puti(w.value, 2)  -> prints 42
//   puti(@ptr, 2)     -> prints 42
// good

Wrapper :: struct {
    value: int
}

w: Wrapper
w.value = 42

ptr: int* = ^w.value

puti(w.value, 2)
puti(@ptr, 2)
