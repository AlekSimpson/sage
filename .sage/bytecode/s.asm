

mov r10 r23
sub r23 r23 48
sub r11 r10 40
sub r12 r10 48
sub r13 r10 32
storea 8 $r11 r13
storea 8 $r12 4
sub r14 r24 40
loada 8 r15 $r14
acpy 32 r15 0
mov r16 0
add r16 r16 40
add r16 r16 0
sub r17 r24 r16
loada 8 r18 $r17
loada 16 r19 $r18
mov r0 r19
mov r1 1
call @2090629895
mov r0 32
mov r1 1
call @2090629905
mov r10 0
add r10 r10 40
add r10 r10 8
sub r11 r24 r10
loada 8 r12 $r11
mov r0 r12
mov r1 1
call @2090629895
sub r14 r24 40
loada 8 r16 $r14
mov r17 0
mul r18 r17 8
add r15 r16 r18
loada 8 r19 $r15
mov r0 r19
mov r1 1
call @2090629895
sub r11 r24 40
loada 8 r13 $r11
mov r14 1
mul r15 r14 8
add r12 r13 r15
loada 8 r16 $r12
mov r0 r16
mov r1 1
call @2090629895
sub r18 r24 40
loada 8 r10 $r18
mov r11 2
mul r12 r11 8
add r19 r10 r12
loada 8 r13 $r19
mov r0 r13
mov r1 1
call @2090629895
sub r15 r24 40
loada 8 r17 $r15
mov r18 3
mul r19 r18 8
add r16 r17 r19
loada 8 r10 $r16
mov r0 r10
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
