// Tests that all integer primitive types (i8, i32, i64) can be declared
// alongside each other and used in arithmetic without confusion.
//
// Expected stdout:
//   puti(sum, 4) -> prints 23016  (7 + 9 + 23000)
// good

byte_val: i8 = 7
int32_val: i32 = 9
int64_val: i64 = 23000

sum: i64 = byte_val + int32_val + int64_val
puti(sum, 5)
