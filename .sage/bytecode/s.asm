

label @193489329
sub r125 r0 1
mov r6 r125
ret
label @193495071
add r126 r0 1
mov r6 r126
ret
label @2090499946
mov r0 2
call @193495071
mov r25 r6
mov r0 4
call @193495071
mov r26 r6
mov r0 8
call @193489329
mov r27 r6
mov r0 r25
mov r1 1
call @2090629895
mov r0 r26
mov r1 1
call @2090629895
mov r0 r27
mov r1 1
call @2090629895
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
