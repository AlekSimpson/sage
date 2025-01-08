source_filename = "examples/add.sage"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

declare i32 @printf(i8*, ...)
define void @main() {
entry:
	%ctuucod29jg4e3lmsiag = add i32 2, 2
	%result = alloca i32
	store i32 %ctuucod29jg4e3lmsiag, i32* %result
	@ctuucod29jg4e3lmsib0 = constant [ 6 x i8 ] c"%d\0A\00"
	%ctuucod29jg4e3lmsibg = load i32, i32* %result
	%ctuucod29jg4e3lmsic0 = call i32 (i8*, i32) @printf(i8* getelementptr ([ 6 x i8 ], [ 6 x i8 ]* %ctuucod29jg4e3lmsib0, i32 0, i32 0), i32 %ctuucod29jg4e3lmsibg)
	ret void
}

