

sub r23 r23 24
mov r128 0
add r128 r128 0
add r128 r128 0
sub r129 r24 r128
sub r23 r23 5
mov r130 r23
storea 8 $r129 r130
scpy 5 r130 0
sub r131 r129 8
storea 8 $r131 5
mov r132 0
add r132 r132 0
add r132 r132 16
sub r133 r24 r132
storea 8 $r133 30
mov r134 0
add r134 r134 0
add r134 r134 0
sub r135 r24 r134
mov r0 r135
call @1829159046
mov r25 r6
mov r0 r25
mov r1 1
call @2090629895
mov r136 0
add r136 r136 0
add r136 r136 0
add r136 r136 0
sub r137 r24 r136
loada 8 r138 $r137
mov r0 r138
mov r1 r25
call @2090629905
label @1829159046
mov r125 0
add r125 r125 8
sub r126 r0 r125
loada 8 r127 $r126
mov r6 r127
ret
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
