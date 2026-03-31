// Tests passing a function call as an argument to another function call.
// Verifies that the return value of add_one(5) is correctly passed into double().
//
// Expected stdout:
//   puti(result, 2) -> prints 12
// good

double :: (x: int) -> int {
    ret x * 2
}

add_one :: (x: int) -> int {
    ret x + 1
}

result: int = double(add_one(5))
puti(result, 2)
