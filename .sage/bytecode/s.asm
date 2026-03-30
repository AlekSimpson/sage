

label @1198612761
sub r23 r23 16
sub r23 r23 9
mov r125 r23
store 8 ($fp - 0) r125
scpy 9 r125 0
store 8 ($fp - 8) 9
sub r126 r24 8
sub r127 r6 8
acpy 16 r127 r126
ret
sub r23 r23 16
mov r6 r23
sub r23 r23 16
call @1198612761
sub r128 r24 8
sub r129 r6 8
acpy 16 r128 r129
mov r130 0
add r130 r130 0
add r130 r130 8
sub r131 r24 r130
loada 8 r132 $r131
mov r0 0
mov r1 r132
call @2090629905
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
