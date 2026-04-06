; lesson1.asm - Basic Arithmetic & Jumps
ORG 0000h

0000  3E 0A      LD A, 0Ah
0002  3C         INC A
0003  3C         INC A
0004  C3 00 00   JP 0000h
