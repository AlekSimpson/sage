#execute {
    executable_name = "output"
    platform = "LINUX"
    architecture = "X86"
    bitsize = 64

    include_files = [
	"some_package", 
	"some_other_package",
    ]
}

// '--' symbol means keep the value uninitialized
Car :: struct {
    name:          string = "default",
    price:         i16 = 1300,
    plate_num:     string = "34A0",
    service_years: [i32 : 4] = [0, 0, 0, 0],
    next_car:      Car* = --,
}

// the 'compact' keyword makes it so that this data structure will be stored in memory as a structure of arrays
// no special indexing is required to work with NumberCache objects, the syntax stays the same
// the only thing that changes is how the memory is laid out under the hood
// Note: when a heap NumberCache is allocated this creates another NumberCache SOA container on the heap
//       this way there is no weird edge cases that come up with mixed allocations types for the same SOA container
NumberCache :: compact struct {
   float_value: float,
   int_value: int,
   int32_value: i32,

   using car : Car,
}

add :: (x: int, y: int) -> int {
    ret x + y
}

mult :: (x: int, y: int) -> int {
    // 'do_mult' captures the value 'result', capturing means that the function will search for the 
    // captured variable in the current scope and use that value in the function
    // we do not have to capture 'result', we could omit the '[result]' notation and the function 
    // would still run and it would still search for result in the outer scopes like normal 
    // the point of capturing values is that this way if or when we want to promote this function to a higher scope we 
    // have a way of documenting to the programmer that this function depends on currently in scope variables that 
    // should probably become parameters if the function is moved to a higher scope.
    do_mult :: [result] (end: int, num: int) {
	// 'it' is a built in iterator variable that is initialized in every for loop scope
	// holds either the index or iterator object, depending on the loop
	// 'it' is not initialized if another named iterator variable is specified
	for 0...end {
	    result = result * it + num
	}
	ret result
    }

    result int
    do_mult(50, x)
    do_mult(4, y)

    ret result
}

honk :: (using car : Car) -> void {
    message: string = "honk!\n"
    printf("%s honked!\n", name) // printf is builtin, no imports required
}

main :: () {
    car : Car!* = new Car // the '!' pointer parameter tells the compiler that it owns this memory and it will free the memory when the current scope closes
    car.name = "BYD"

    car_two := new Car
    car_two.name = "Xiaomi"

    car.next_car = car_two

    car.next_char.honk() // prints: 'Xiaomi honked!\n'

    // arrays
    dynamic_array: [int : ..]
    dynamic_array.append(5) 

    static_array: [int : 4] = --
    static_array[0] = 9

    
    // no need to free the first car because it will be auto freed when the main function ends
    free car_two 
}


--------------------------------------------------------------------------------
CFG:

S               -> program

STAR            -> *STAR | EMPTYSTRING
array           -> [ primitive ] | [ ID ] | [ primitive : <digits> ] | [ ID : <digits> ]
primitive       -> int | i.{16, 32, 64} | float | f.{16, 32, 64} | char | array | void
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
6. Arbitrary Compile Time Code Execution, using bytecode interpreter
7. Functions as first class citizens

- ternary expressions
- Automated AST Manipulation / Refactoring
- the ':=' operator
- infix array initialization, ex: ages [int:5] = [50, 20, 1, 17, 80]
- 'fallthrough' keyword
- keyword 'using' before function parameters means that the scope of that value is injected into the scope of the function


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


=============================================

#execute {
     executable_name = "output"
     platform = "LINUX"
     architecture = "X86"
     bitsize = 64
}

add:: (x: int, y: int) -> int {
    return x + y
}

main:: () -> void {
    result: i64 = add(2, 2) * 10
    printf("this result is : %d\n", result)
}

=============================================

begin // begin compile time execution

add stack_pointer, stack_pointer, 1
store "output", stack_pointer

add stack_pointer, stack_pointer, 1
store "LINUX", stack_pointer

add stack_pointer, stack_pointer, 1
store "X86", stack_pointer

add stack_pointer, stack_pointer, 1
store 64, stack_pointer

end // end compile time execution

add:
  add sr12, sr0, sr1
  mov sr12, sr6
  ret

main:
  mov 2, sr0
  mov 2, sr1
  call add

  mul sr9, sr6, 10

  add stack_pointer, stack_pointer, 1
  store sr9, stack_pointer

  load sr10, sr9 // sr9 value is auto treated as a stack pointer

  // idea: honestly maybe print and all the other builtins could be a "syscall"
  print "this result is: %d\n", sr10

  mov 0, sr22 // 0 is the program exit syscall code
  syscall




































