// Tests summing array elements using element-by-element addition stored in a local.
// Verifies that multiple indexed reads from the same array work correctly.
//
// Expected stdout:
//   puti(total, 2) -> prints 15
// good

nums: i64[5] = [1, 2, 3, 4, 5]
total: i64 = nums[0] + nums[1] + nums[2] + nums[3] + nums[4]
puti(total, 2)
