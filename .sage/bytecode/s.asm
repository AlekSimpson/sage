

sub r23 r23 16
mov r125 0
add r125 r125 0
add r125 r125 0
sub r126 r24 r125
storea 8 $r126 99
sub r23 r23 16
mov r127 0
add r127 r127 16
add r127 r127 0
sub r128 r24 r127
storea 8 $r128 2
mov r129 0
add r129 r129 16
add r129 r129 8
sub r130 r24 r129
loadr r131 ($fp - 0)
storea 8 $r130 r131
sub r23 r23 16
mov r132 0
add r132 r132 32
add r132 r132 0
sub r133 r24 r132
storea 8 $r133 1
mov r134 0
add r134 r134 32
add r134 r134 8
sub r135 r24 r134
loadr r136 ($fp - 16)
storea 8 $r135 r136
mov r137 0
add r137 r137 32
add r137 r137 8
sub r138 r24 r137
loada 8 r139 $r138
mov r137 0
add r137 r137 8
sub r140 r139 r137
loada 8 r141 $r140
mov r137 0
add r137 r137 0
sub r142 r141 r137
loada 8 r143 $r142
mov r0 r143
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
