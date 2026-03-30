

label @340279201
loada 8 r125 $r6
mov r126 0
acpy 24 r125 r126
ret
mov r127 r23
mov r128 r23
sub r128 r23 8
sub r23 r23 40
mov r129 r23
storea 8 $r127 r129
storea 8 $r128 3
mov r130 r23
sub r23 r23 40
mov r131 r23
storea 8 $r130 r131
sub r132 r130 8
storea 8 $r132 3
mov r6 r130
call @340279201
sub r133 r24 0
loada 8 r134 $r133
loada 8 r135 $r6
acpy 24 r134 r135
sub r137 r24 0
loada 8 r139 $r137
mov r140 0
mul r141 r140 8
add r138 r139 r141
loada 8 r142 $r138
mov r0 r142
mov r1 1
call @2090629895
sub r144 r24 0
loada 8 r146 $r144
mov r147 1
mul r148 r147 8
add r145 r146 r148
loada 8 r149 $r145
mov r0 r149
mov r1 1
call @2090629895
sub r151 r24 0
loada 8 r153 $r151
mov r154 2
mul r155 r154 8
add r152 r153 r155
loada 8 r156 $r152
mov r0 r156
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
