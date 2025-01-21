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


--------------------------------------------------------------------------------
CFG:

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
                   expression |
                   compile-time-run-stmt

value_dec       -> ID TYPE
assign          -> ID = expression | ID TYPE = expression
value_dec_list  -> value_dec , value_dec_list | value_dec , | value_dec

range           -> expression ... expression | ( expression ... expression )
return          -> ret expression | ret

binding         -> :: funcdef | :: structdef | :: TYPE
construct       -> ID binding

funcdef         -> ( value_dec_list ) -> TYPE body | ( ) -> TYPE body | ( ) -> TYPE | ( value_dec_list ) -> TYPE
structdef       -> struct { value_dec_list }
for_stmt        -> for ID in range body // TODO: we should support more for loop formats
while_stmt      -> while expression body
if_stmt         -> if expression body elif_stmt
elif_stmt       -> else if expression body elif_stmt |
                   else if expression body else body |
                   else if expression body |
                   EMPTYSTRING

compile-time-run-stmt -> #run body

expression -> recursive descent on lang operators

note: binding to a type means creating a constant value note: primary refers to the primary level of expression parsing which i have not written out here because it would just be boiler plate to write`

VISION:

1. *seemless and useful* LLM integration
2. No OOP; only structs
3. Polymorphic functions
4. C like memory management. No smart pointers. No complicated borrow rules. No garbage collection.
5. The build system is built into the compiler
6. Arbitrary Compile Time Code Execution
7. Functions as first class citizens

- eventually i want to add macros
- also want to add function pattern matching definition types like in haskell, 
	ex: 
	and 1 1 = 1 
	and 0 1 = 0 
	and 1 0 = 0 
	and 0 0 = 0 
	and _ _ = 0
- ternary expressions
- Automated AST Manipulation / Refactoring
- the ':=' operator
- infix array initialization, ex: ages [int:5] = [50, 20, 1, 17, 80]
- 'fallthrough' keyword
- keyword 'using' before function parameters means that the scope of that value is injected into the scope of the function, (this probably could also be consisted with library imports)


target datalayouts for the most common platforms:
Linux x86_64:
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

Linux x86 (32-bit):
target datalayout = "e-m:e-p:32:32-f64:32:64-f80:32-n8:16:32-S128"
target triple = "i686-unknown-linux-gnu"

macOS x86_64:
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-darwin"

macOS ARM64 (Apple Silicon):
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "arm64-apple-darwin"

Windows x64:
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-windows-msvc"

Windows x86 (32-bit):
target datalayout = "e-m:x-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "i686-pc-windows-msvc"

---------------------------------------------------------------------------------------
Compiler CLI ideas:
"sage infuse ..." - Could be for importing or including dependencies
"sage pour ..." - For outputting or exporting ir code
"sage steep <optimization-level> ..." - Could control compilation optimization levels
    - "sage steep light ..." for -O1
    - "sage steep medium ..." for -O2
    - "sage steep strong ..." for -O3
    - "sage steep none ..." for -O0
"sage chai ..." - For checking/testing code
"sage decant ..." - For deploying or distributing compiled code
"sage sift ..." - For linting or static analysis
"sage simmer ..." - For running in debug mode
"sage steward ..." - For package management
"sage distill ..." - For creating minimal builds

not confirmed ideas:
"sage harest"
"sage prune ..." - For removing unused code or dependencies
"sage graft ..." - For integrating new modules or features
"sage cultivate ..." - For initializing a new project
"sage propagate ..." - For copying or duplicating code components
"sage mulch ..." - For managing build artifacts or caching
"sage repot ..." - For migrating or restructuring code
"sage pinch ..." - For trimming debug statements or comments
"sage mist ..." - For running light debug checks
"sage root ..." - For dependency resolution
"sage shape ..." - For code formatting
"sage feed ..." - For updating dependencies
"sage clip ..." - For creating smaller builds or snippets
"sage thin ..." - For code optimization
"sage bind ..." - For tying modules together (like binding branches in bonsai)
"sage split ..." - For breaking apart modules (like air-layering a bonsai)



- BLOCK_NODE
	- UNARY NODE (COMPILE_TIME_EXECUTE: #run { ... } | BLOCK_NODE)
		- BLOCK_NODE
			- BINARY NODE (ASSIGN: build_settings.executable_name = "output" | build_settings | "output")
				- LIST NODE (LIST: build_settings.executable_name)
				- UNARY NODE (STRING: "output")
			- BINARY NODE (ASSIGN: build_settings.platform = "LINUX" | build_settings | "LINUX")
				- LIST NODE (LIST: build_settings.platform)
				- UNARY NODE (STRING: "LINUX")
			- BINARY NODE (ASSIGN: build_settings.architecture = "X86" | build_settings | "X86")
				- LIST NODE (LIST: build_settings.architecture)
				- UNARY NODE (STRING: "X86")
			- BINARY NODE (ASSIGN: build_settings.bitsize = 64 | build_settings | 64)
				- LIST NODE (LIST: build_settings.bitsize)
				- UNARY NODE (NUMBER: 64)
	- BINARY NODE (FUNCDEF: main :: (Empty Parameter List) -> void | main | (Empty Parameter List) -> void)
		- UNARY NODE (IDENTIFIER: main)
		- TRINARY NODE (FUNCDEF: (Empty Parameter List) -> void | Empty Parameter List | void | { ... })
			- BLOCK_NODE
			- UNARY NODE (TYPE: void)
			- BLOCK_NODE
				- TRINARY NODE (ASSIGN: result int = BINARY NODE (BINARY: + | 2 | 2) | result | int | +)
					- UNARY NODE (IDENTIFIER: result)
					- UNARY NODE (TYPE: int)
					- BINARY NODE (BINARY: + | 2 | 2)
						- UNARY NODE (NUMBER: 2)
						- UNARY NODE (NUMBER: 2)
				- UNARY NODE (FUNCCALL: printf | BLOCK_NODE)
					- BLOCK_NODE
						- UNARY NODE (STRING: "%d\n")
						- UNARY NODE (VAR_REF: result)
































