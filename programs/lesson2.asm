; lesson2.asm - Memory Pokes
0000  06 32      LD B, 32h
0002  78         LD A, B
0003  32 00 70   LD (7000h), A
0006  76         HALT
