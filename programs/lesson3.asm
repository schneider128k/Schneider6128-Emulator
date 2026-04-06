; lesson3.asm - Stack and Subroutine Test
ORG 0000h

0000  31 00 F0   LD SP, F000h
0003  3E 05      LD A, 05h
0005  CD 09 00   CALL 0009h
0008  76         HALT

; --- Subroutine ---
0009  3C         INC A
000A  C9         RET
