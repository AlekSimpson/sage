// Tests a void function (no return type) that uses several local variables
// and calls puti internally. Verifies void functions clean up the stack correctly.
//
// Expected stdout:
//   puti(product, 2) -> prints 200
// good

print_product :: (a: int, b: int) {
    product: int = a * b
    puti(product, 3)
}

print_product(10, 20)
