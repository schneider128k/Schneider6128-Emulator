# programs/gen_lesson3.py

program_name = "lesson3"

# Z80 Machine Code Bytes:
# 31 00 F0 -> LD SP, 0xF000 (Initialize stack)
# 3E 05    -> LD A, 5       (Load 5 into Accumulator)
# CD 09 00 -> CALL 0x0009   (Call our subroutine)
# 76       -> HALT          (Stop the CPU)
# 3C       -> INC A         (Subroutine: Increment A)
# C9       -> RET           (Return to main)

binary_data = bytes([
    0x31, 0x00, 0xF0, # LD SP, 0xF000
    0x3E, 0x05,       # LD A, 0x05
    0xCD, 0x09, 0x00, # CALL 0x0009
    0x76,             # HALT
    0x3C,             # INC A (at address 0x0009)
    0xC9              # RET   (at address 0x000A)
])

# 1. Write the BIN file for the Emulator
with open(f"programs/{program_name}.bin", "wb") as f:
    f.write(binary_data)

# 2. Write the ASM file for GitHub Readability
with open(f"programs/{program_name}.asm", "w") as f:
    f.write(f"; {program_name}.asm - Stack and Subroutine Test\n")
    f.write("ORG 0000h\n\n")
    f.write("0000  31 00 F0   LD SP, F000h\n")
    f.write("0003  3E 05      LD A, 05h\n")
    f.write("0005  CD 09 00   CALL 0009h\n")
    f.write("0008  76         HALT\n\n")
    f.write("; --- Subroutine ---\n")
    f.write("0009  3C         INC A\n")
    f.write("000A  C9         RET\n")

print(f"Successfully generated {program_name}.bin and {program_name}.asm in /programs")