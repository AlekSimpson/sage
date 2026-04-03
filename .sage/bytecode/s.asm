

sub r23 r23 16
sub r23 r23 5
mov r129 r23
store 8 ($fp - 0) r129
scpy 5 r129 0
store 8 ($fp - 8) 5
mov r130 0
add r130 r130 0
add r130 r130 8
sub r131 r24 r130
loada 8 r132 $r131
load 8 r0 ($fp - 0)
mov r1 r132
call @2090629905
call @2090499946
label @2090499946
sub r23 r23 16
sub r23 r23 4
mov r125 r23
store 8 ($fp - 0) r125
scpy 4 r125 5
store 8 ($fp - 8) 4
mov r126 0
add r126 r126 0
add r126 r126 8
sub r127 r24 r126
loada 8 r128 $r127
load 8 r0 ($fp - 0)
mov r1 r128
call @2090629905
exit
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
