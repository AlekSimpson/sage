// Tests using string.bytes directly as the first argument to puts,
// mirroring how structs_four accesses .bytes on a struct's string field.
// Verifies .bytes is a valid address-like value usable at a call site.
//
// Expected stdout:
//   puts(s.bytes, s.length) -> prints "compiler"
// good

s: string = "compiler"
puts(s.bytes, s.length)
