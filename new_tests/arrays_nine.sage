// Tests using an array's .length property inside an arithmetic expression,
// ensuring .length is treated as a proper value, not just a printable literal.
//
// Expected stdout:
//   puti(doubled_length, 2) -> prints 10
//   puti(nums[2], 2)        -> prints 30
// good

nums: i64[5] = [10, 20, 30, 40, 50]
doubled_length: i64 = nums.length * 2
puti(doubled_length, 2)
puti(nums[2], 2)
