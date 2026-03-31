// Tests two top-level functions that call each other in sequence (not mutually
// recursive, just sequentially). Verifies independent call frames don't bleed.
//
// Expected stdout:
//   puti(sum, 2)  -> prints 15
//   puti(diff, 2) -> prints 5
// good

add :: (x: int, y: int) -> int {
    ret x + y
}

sub :: (x: int, y: int) -> int {
    ret x - y
}

sum: int = add(10, 5)
diff: int = sub(10, 5)

puti(sum, 2)
puti(diff, 2)
