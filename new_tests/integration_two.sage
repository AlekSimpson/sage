// Integration: array element accessed inside a function body (local array),
// combined with a global constant. Mixes local array, global constant, and
// function return in one computation.
//
// Expected stdout:
//   puti(result, 2) -> prints 106  (100 + arr[1])  arr[1]=6
// good

OFFSET: i64 = 100

get_second :: () -> int {
    arr: int[3] = [4, 6, 8]
    ret arr[1]
}

result: int = OFFSET + get_second()
puti(result, 3)
