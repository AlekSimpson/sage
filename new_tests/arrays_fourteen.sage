// Tests arithmetic using a string array element's .length property.
// Verifies that .length on an indexed string element is a usable integer value.
//
// Expected stdout:
// "hi".length = 2, "there".length = 5, sum = 7
// puti(len_sum, 2) -> prints 7
// good

words: string[2] = ["hi", "there"]
len_sum: int = words[0].length + words[1].length
puti(len_sum, 1)
