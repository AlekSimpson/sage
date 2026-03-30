

sub r23 r23 16
mov r0 5
mov r1 6
mov r6 r23
sub r23 r23 16
call @1297703093
sub r131 r24 8
sub r132 r6 8
acpy 16 r131 r132
mov r133 0
add r133 r133 0
add r133 r133 0
sub r134 r24 r133
loada 8 r135 $r134
mov r0 r135
mov r1 1
call @2090629895
mov r136 0
add r136 r136 0
add r136 r136 8
sub r137 r24 r136
loada 8 r138 $r137
mov r0 r138
mov r1 1
call @2090629895
label @1297703093
sub r23 r23 16
mov r125 0
add r125 r125 0
add r125 r125 0
sub r126 r24 r125
storea 8 $r126 r0
mov r127 0
add r127 r127 0
add r127 r127 8
sub r128 r24 r127
storea 8 $r128 r1
sub r129 r24 8
sub r130 r6 8
acpy 16 r130 r129
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
