

label @1065202816
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
mov r131 r23
mov r132 r23
sub r132 r23 8
sub r23 r23 48
mov r133 r23
storea 8 $r131 r133
storea 8 $r132 2
sub r135 r24 0
loada 8 r137 $r135
mov r138 0
mul r139 r138 16
add r136 r137 r139
mov r0 1
mov r1 2
mov r6 r23
sub r23 r23 16
call @1065202816
sub r140 r6 0
add r141 r136 0
acpy 8 r141 r140
sub r142 r6 8
add r143 r136 8
acpy 8 r143 r142
sub r145 r24 0
loada 8 r147 $r145
mov r148 1
mul r149 r148 16
add r146 r147 r149
mov r0 3
mov r1 4
mov r6 r23
sub r23 r23 16
call @1065202816
sub r150 r6 0
add r151 r146 0
acpy 8 r151 r150
sub r152 r6 8
add r153 r146 8
acpy 8 r153 r152
sub r155 r24 0
loada 8 r157 $r155
mov r158 0
mul r159 r158 16
add r156 r157 r159
mov r160 0
add r160 r160 0
add r161 r156 r160
loada 8 r162 $r161
mov r0 r162
mov r1 1
call @2090629895
sub r164 r24 0
loada 8 r166 $r164
mov r167 0
mul r168 r167 16
add r165 r166 r168
mov r169 0
add r169 r169 8
add r170 r165 r169
loada 8 r171 $r170
mov r0 r171
mov r1 1
call @2090629895
sub r173 r24 0
loada 8 r175 $r173
mov r176 1
mul r177 r176 16
add r174 r175 r177
mov r178 0
add r178 r178 0
add r179 r174 r178
loada 8 r180 $r179
mov r0 r180
mov r1 1
call @2090629895
sub r182 r24 0
loada 8 r184 $r182
mov r185 1
mul r186 r185 16
add r183 r184 r186
mov r187 0
add r187 r187 8
add r188 r183 r187
loada 8 r189 $r188
mov r0 r189
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
