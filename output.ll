source_filename = "examples/add.sage"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@cu06iqd29jg65g2k1j0g = constant [ 4 x i8 ] c"%d\0A\00"
@cu06iqd29jg65g2k1j20 = constant [ 13 x i8 ] c"Hello world\0A\00"
@cu06iqd29jg65g2k1j30 = constant [ 10 x i8 ] c"hello %s\0A\00"
@cu06iqd29jg65g2k1j3g = constant [ 5 x i8 ] c"sage\00"

declare i32 @printf(i8*, ...)


define void @main() {
entry:
	%cu06iqd29jg65g2k1j00 = add i32 2, 2
	%result = alloca i32
	store i32 %cu06iqd29jg65g2k1j00, i32* %result
	%cu06iqd29jg65g2k1j10 = load i32, i32* %result
	%cu06iqd29jg65g2k1j1g = call i32 (i8*, ...) @printf(i8* getelementptr ([ 4 x i8 ], [ 4 x i8 ]* @cu06iqd29jg65g2k1j0g, i32 0, i32 0), i32 %cu06iqd29jg65g2k1j10)
	%cu06iqd29jg65g2k1j2g = call i32 (i8*, ...) @printf(i8* getelementptr ([ 13 x i8 ], [ 13 x i8 ]* @cu06iqd29jg65g2k1j20, i32 0, i32 0))
	%cu06iqd29jg65g2k1j40 = call i32 (i8*, ...) @printf(i8* getelementptr ([ 10 x i8 ], [ 10 x i8 ]* @cu06iqd29jg65g2k1j30, i32 0, i32 0), i8* getelementptr ([ 5 x i8 ], [ 5 x i8 ]* @cu06iqd29jg65g2k1j3g, i32 0, i32 0))
	ret void
}

