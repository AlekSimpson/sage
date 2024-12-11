// ALL VARIABLES ARE IMMUTABLE BY DEFAULT

include "stdio.g"

Car :: struct {
    string  name,
    int16   price,
    string  plate_num,
    [int32] serviceYears,
    Car*    nextCar,
}

add :: (int x, int y) -> int {
    ret x + y
}

mult :: (int x, int y) -> int {
    mut int result
    for x in (0...50) {
        result++x
    }
    ret result
}

honk :: () -> void {
    printf("honk!\n")
}




STAR            -> *STAR | EMPTYSTRING
primitive       -> int | char | array | void
pointer         -> TYPE STAR
TYPE            -> primitive | pointer

program         -> libraries body 
libraries       -> library libraries | library | EMPTYSTRING
library         -> include string NEWLINE | EMPTYSTRING

body            -> { statement_list }
statement_list  -> statement statement_list | statement | EMPTYSTRING
statement       -> value_assign | value_init | assign | return | construct | for | while | expression

value_assign    -> TYPE ID = expression
value_init      -> TYPE ID
value_init_list -> value_init , value_init_list | value_init , | value_init
assign          -> ID = expression
range           -> expression ... expression | ( expression ... expression )
return          -> ret expression
binding         -> :: funcdef | :: structdef | :: TYPE
construct       -> ID binding

funcdef         -> ( value_init_list ) -> TYPE body | ( ) -> TYPE body
structdef       -> struct { value_init_list }
for             -> for ID in range body
while           -> while expression body

expression -> addop

VISION:

1. *seemless and useful* LLM integration
2. No OOP; only structs
3. Polymorphic functions
4. C like memory management. No smart pointers. No complicated borrow rules. No garbage collection.
5. The build system is built into the compiler
6. Arbitrary Compile Time Code Execution
7. Functions as first class citizens
