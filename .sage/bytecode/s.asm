

mov r125 r23
mov r126 r23
sub r126 r23 8
sub r23 r23 80
mov r127 r23
storea 8 $r125 r127
storea 8 $r126 4
load 8 r128 ($fp - 0)
mov r129 0
acpy 64 r128 r129
sub r131 r24 0
loada 8 r133 $r131
mov r134 0
mul r135 r134 16
add r132 r133 r135
loada 16 r136 $r132
sub r138 r24 0
loada 8 r140 $r138
mov r141 0
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
mov r150 1
mul r151 r150 16
add r148 r149 r151
loada 16 r152 $r148
sub r154 r24 0
loada 8 r156 $r154
mov r157 1
mul r158 r157 16
add r155 r156 r158
mov r159 0
add r159 r159 8
add r160 r155 r159
loada 8 r161 $r160
mov r0 r152
mov r1 r161
call @2090629905
sub r163 r24 0
loada 8 r165 $r163
mov r166 2
mul r167 r166 16
add r164 r165 r167
loada 16 r168 $r164
sub r170 r24 0
loada 8 r172 $r170
mov r173 2
mul r174 r173 16
add r171 r172 r174
mov r175 0
add r175 r175 8
add r176 r171 r175
loada 8 r177 $r176
mov r0 r168
mov r1 r177
call @2090629905
sub r179 r24 0
loada 8 r181 $r179
mov r182 3
mul r183 r182 16
add r180 r181 r183
loada 16 r184 $r180
sub r186 r24 0
loada 8 r188 $r186
mov r189 3
mul r190 r189 16
add r187 r188 r190
mov r191 0
add r191 r191 8
add r192 r187 r191
loada 8 r193 $r192
mov r0 r184
mov r1 r193
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
