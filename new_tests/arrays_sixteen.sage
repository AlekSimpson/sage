// Tests using array.first in an arithmetic expression (not just dereferencing it).
// Verifies .first is a usable address value that can participate in arithmetic.
//
// Expected stdout:
//   puti(@arr.first, 1) -> prints 9
//   puti(arr.length + arr[1], 1) -> prints 6  (length=4, arr[1]=2)
// good

arr: int[4] = [9, 2, 3, 4]
puti(@arr.first, 1)

length_plus_second: int = arr.length + arr[1]
puti(length_plus_second, 1)
