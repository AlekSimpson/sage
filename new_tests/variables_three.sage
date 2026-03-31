// Tests a large number of global variables all initialized to literal values,
// then summed. Stresses the symbol table and code-generation paths for
// many top-level declarations.
//
// Expected stdout:
//   puti(total, 2) -> prints 45   (1+2+3+4+5+6+7+8+9)
// good

a: int = 1
b: int = 2
c: int = 3
d: int = 4
e: int = 5
f: int = 6
g: int = 7
h: int = 8
i: int = 9

total: int = a + b + c + d + e + f + g + h + i
puti(total, 2)
