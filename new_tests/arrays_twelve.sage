// Tests overwriting every element of an array and then reading them all back.
// Stresses that element writes are independent and don't alias each other.
//
// Expected stdout:
//   puti(arr[0], 1) -> prints 10
//   puti(arr[1], 1) -> prints 20
//   puti(arr[2], 1) -> prints 30
// good

arr: i64[3] = [1, 2, 3]

arr[0] = 10
arr[1] = 20
arr[2] = 30

puti(arr[0], 2)
puti(arr[1], 2)
puti(arr[2], 2)
