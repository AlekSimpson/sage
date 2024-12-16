include "stdio.g"

Car :: struct {
    name          string, 
    price         int16,
    plate_num     string,
    service_years [int32],
    next_car      Car*,
}

add :: (x int, y int) -> int {
    ret x + y
}

mult :: (x int, y int) -> int {
    result int
    for x in (0...50) {
        result++x
        result--4
    }
    ret result
}

honk :: () -> void {
    message string = "honk!\n"
    printf(message)
}



S               -> program

STAR            -> *STAR | EMPTYSTRING
array           -> [ primitive ] | [ ID ]
primitive       -> int | char | array | void
pointer         -> TYPE STAR
TYPE            -> primitive | pointer

program         -> libraries statements | statements

library         -> include string NEWLINE | EMPTYSTRING
libraries       -> library libraries | library | EMPTYSTRING

body            -> { statements }
statements      -> statement statements | statement | EMPTYSTRING
statement       -> value_dec  |
                   assign     |
                   return     |
                   construct  |
                   if_stmt    |
                   for_stmt   |
                   while_stmt |
                   expression

value_dec       -> ID TYPE
assign          -> ID = expression | ID TYPE = expression
value_dec_list  -> value_dec , value_dec_list | value_dec , | value_dec

range           -> expression ... expression | ( expression ... expression )
return          -> ret expression

binding         -> :: funcdef | :: structdef | :: TYPE
construct       -> ID binding

funcdef         -> ( value_dec_list ) -> TYPE body | ( ) -> TYPE body | ( ) -> TYPE | ( value_dec_list ) -> TYPE
structdef       -> struct { value_dec_list }
for_stmt        -> for ID in range body
while_stmt      -> while expression body
if_stmt         -> if expression body elif_stmt
elif_stmt       -> else if expression body elif_stmt |
                   else if expression body else body |
                   else if expression body |
                   EMPTYSTRING

expression -> recursive descent on lang operators

VISION:

1. *seemless and useful* LLM integration
2. No OOP; only structs
3. Polymorphic functions
4. C like memory management. No smart pointers. No complicated borrow rules. No garbage collection.
5. The build system is built into the compiler
6. Arbitrary Compile Time Code Execution
7. Functions as first class citizens
