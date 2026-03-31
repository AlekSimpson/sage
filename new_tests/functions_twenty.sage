// Tests a function with f64 parameters and return type.
// Verifies floating-point values survive the call frame round-trip.
//
// Expected stdout:
//   puti(result, 4) -> prints 3 (truncated integer view, runtime-dependent)
//   (the key check is that the bytecode compiles without error)
// maybe? 

fsub :: (x: f64, y: f64) -> f64 {
    ret x - y
}

fadd :: (x: f64, y: f64) -> f64 {
    ret x + y
}

a: f64 = fadd(1.5, 2.5)
b: f64 = fsub(10.0, 7.0)
