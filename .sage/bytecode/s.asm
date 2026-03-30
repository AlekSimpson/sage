

mov r125 r23
mov r126 r23
sub r126 r23 8
sub r23 r23 48
mov r127 r23
storea 8 $r125 r127
storea 8 $r126 2
sub r129 r24 0
loada 8 r131 $r129
mov r132 0
mul r133 r132 16
add r130 r131 r133
mov r134 0
add r134 r134 0
add r135 r130 r134
storea 8 $r135 5
sub r137 r24 0
loada 8 r139 $r137
mov r140 0
mul r141 r140 16
add r138 r139 r141
mov r142 0
add r142 r142 8
add r143 r138 r142
storea 8 $r143 897846
sub r145 r24 0
loada 8 r147 $r145
mov r148 1
mul r149 r148 16
add r146 r147 r149
mov r150 0
add r150 r150 0
add r151 r146 r150
storea 8 $r151 20
sub r153 r24 0
loada 8 r155 $r153
mov r156 1
mul r157 r156 16
add r154 r155 r157
mov r158 0
add r158 r158 8
add r159 r154 r158
storea 8 $r159 56
sub r161 r24 0
loada 8 r163 $r161
mov r164 0
mul r165 r164 16
add r162 r163 r165
mov r166 0
add r166 r166 8
add r167 r162 r166
loada 8 r168 $r167
mov r0 r168
mov r1 6
call @2090629895
mov r0 0
call @2090629905
sub r170 r24 0
loada 8 r172 $r170
mov r173 1
mul r174 r173 16
add r171 r172 r174
mov r175 0
add r175 r175 8
add r176 r171 r175
loada 8 r177 $r176
mov r0 r177
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
