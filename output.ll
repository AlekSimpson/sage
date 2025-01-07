source_filename = "examples/add.sage"
target datalayout = ""
target triple = ""

declare i32 @printf(i8* %input, ...)
define void @main() {
entry:

	%ctu87r529jg5ebkaro10 = add i32 2, 2
	%result = alloca i32
	store i32 %ctu87r529jg5ebkaro10, i32* %result
	%ctu87r529jg5ebkaro1g = constant [ 6 x i8 ] c"%d\n"
	%ctu87r529jg5ebkaro2g = load i32, i32* %result
	%ctu87r529jg5ebkaro30 = call i32 @printf([ 6 x i8 ] %ctu87r529jg5ebkaro20, i32 %ctu87r529jg5ebkaro2g, )
}

