// Tests that global :: constants and global : variables can be freely mixed
// in the same expression at global scope.
//
// Expected stdout:
//   puti(total, 4) -> prints 1056  (1000 + 56)
// good

LARGE: int = 1000
small: int = 56
total: int = LARGE + small
puti(total, 4)
