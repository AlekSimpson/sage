

sub r23 r23 8
mov r125 0
add r125 r125 0
add r125 r125 0
sub r126 r24 r125
storea 8 $r126 7
mov r127 0
add r127 r127 0
add r127 r127 0
sub r128 r24 r127
loada 8 r129 $r128
mov r0 r129
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
