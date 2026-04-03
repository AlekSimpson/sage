

label @791503889
sub r23 r23 8
mov r128 0
add r128 r128 0
add r128 r128 0
sub r129 r24 r128
storea 8 $r129 r0
load 8 r6 ($fp - 0)
ret
mov r125 r23
mov r126 r23
sub r126 r23 8
sub r23 r23 32
mov r127 r23
storea 8 $r125 r127
storea 8 $r126 2
sub r131 r24 0
loada 8 r133 $r131
mov r134 0
mul r135 r134 8
add r132 r133 r135
mov r0 10
call @791503889
storea 8 $r132 r6
sub r137 r24 0
loada 8 r139 $r137
mov r140 1
mul r141 r140 8
add r138 r139 r141
mov r0 20
call @791503889
storea 8 $r138 r6
sub r143 r24 0
loada 8 r145 $r143
mov r146 0
mul r147 r146 8
add r144 r145 r147
mov r148 0
add r148 r148 0
add r149 r144 r148
loada 8 r150 $r149
mov r0 r150
mov r1 2
call @2090629895
sub r152 r24 0
loada 8 r154 $r152
mov r155 1
mul r156 r155 8
add r153 r154 r156
mov r157 0
add r157 r157 0
add r158 r153 r157
loada 8 r159 $r158
mov r0 r159
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
