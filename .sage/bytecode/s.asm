

sub r23 r23 8
sub r23 r23 16
mov r10 0
add r10 r10 0
loadr r11 ($fp - 8)
store 8 ($fp - 10) r11
mov r12 8
add r12 r12 0
store 8 ($fp - 12) 40
mov r13 8
add r13 r13 8
store 8 ($fp - 13) 100034
mov r14 0
add r14 r14 0
load 16 r15 ($fp - r14)
mov r14 r15
add r14 r14 8
load 8 r16 ($fp - r14)
mov r0 r16
mov r1 6
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
