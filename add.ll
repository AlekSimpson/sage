ource_filename = "add"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; External declaration of printf
declare i32 @printf(i8*, ...)



; Main function
define void @main() {
entry:
    %result = add i32 5, 3

    %format = constant [4 x i8] c"%d\0A\00"
    
    %printf_ret = call i32 (i8*, ...) @printf(
        i8* getelementptr ([4 x i8], [4 x i8]* @format, i64 0, i64 0),
        i32 %result)
    
    ret void
}
