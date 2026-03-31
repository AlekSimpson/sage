

sub r23 r23 8
sub r23 r23 8
store 8 ($fp - 0) 5
loadr r125 ($fp - 0)
store 8 ($fp - 8) r125
load 8 r0 ($fp - 0)
mov r1 1
call @2090629895
load 8 r126 ($fp - 8)
storea 8 $r126 99
load 8 r0 ($fp - 0)
mov r1 2
call @2090629895
label @2090629895
mov r22 1
mov r10 r1
mov r1 r0
mov r2 r10
mov r0 1
syscall
ret
label @2090629905
mov r22 0
mov r10 r1
mov r1 r0
mov r2 r10
mov r0 1
syscall
ret
