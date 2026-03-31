// Tests an array of structs where multiple fields on multiple elements
// are all written and then read back. Extends arrays_eight with more
// reads to ensure field indexing stays correct across elements.
//
// Expected stdout:
//   puti(pts[0].x, 2) -> prints 1
//   puti(pts[0].y, 2) -> prints 2
//   puti(pts[1].x, 2) -> prints 3
//   puti(pts[1].y, 2) -> prints 4
//   puti(pts[2].x, 2) -> prints 5
//   puti(pts[2].y, 2) -> prints 6
// good

Point :: struct {
    x: i64
    y: i64
}

pts: Point[3]

pts[0].x = 1
pts[0].y = 2
pts[1].x = 3
pts[1].y = 4
pts[2].x = 5
pts[2].y = 6

puti(pts[0].x, 1)
puti(pts[0].y, 1)
puti(pts[1].x, 1)
puti(pts[1].y, 1)
puti(pts[2].x, 1)
puti(pts[2].y, 1)
