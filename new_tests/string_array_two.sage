// Tests a string array where every element is overwritten and then read back
// via puts. Extends arrays_six/seven by covering all three positions.
//
// Expected stdout:
//   puts(arr[0]) -> prints "alpha"
//   puts(arr[1]) -> prints "beta"
//   puts(arr[2]) -> prints "gamma"
// good

arr: string[3] = ["one", "two", "three"]

arr[0] = "alpha"
arr[1] = "beta"
arr[2] = "gamma"

puts(arr[0], arr[0].length)
puts(arr[1], arr[1].length)
puts(arr[2], arr[2].length)
