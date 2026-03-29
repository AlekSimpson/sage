

mov r125 r23
mov r126 r23
sub r126 r23 8
sub r23 r23 64
mov r127 r23
storea 8 $r125 r127
storea 8 $r126 3
load 8 r128 ($fp - 0)
mov r129 0
acpy 48 r128 r129
sub r131 r24 0
loada 8 r133 $r131
mov r134 2
mul r135 r134 16
add r132 r133 r135
loada 16 r136 $r132
sub r138 r24 0
loada 8 r140 $r138
mov r141 2
mul r142 r141 16
add r139 r140 r142
mov r143 0
add r143 r143 8
add r144 r139 r143
loada 8 r145 $r144
mov r0 r136
mov r1 r145
call @2090629905
sub r147 r24 0
loada 8 r149 $r147
mov r150 2
mul r151 r150 16
add r148 r149 r151
sub r23 r23 32
mov r152 r23
storea 8 $r148 r152
scpy 32 r152 59
sub r153 r148 8
storea 8 $r153 4
sub r155 r24 0
loada 8 r157 $r155
mov r158 2
mul r159 r158 16
add r156 r157 r159
loada 16 r160 $r156
sub r162 r24 0
loada 8 r164 $r162
mov r165 2
mul r166 r165 16
add r163 r164 r166
mov r167 0
add r167 r167 8
add r168 r163 r167
loada 8 r169 $r168
mov r0 r160
mov r1 r169
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
