

mov r10 r23
sub r23 r23 48
sub r11 r10 32
sub r12 r10 40
storea 8 $r11 r10
storea 8 $r12 4
mov r13 0
add r13 r13 32
add r13 r13 8
sub r14 r24 r13
loada 8 r15 $r14
mov r0 r15
mov r1 1
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
