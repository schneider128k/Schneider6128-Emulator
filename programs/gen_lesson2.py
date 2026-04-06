# programs/gen_lesson2.py
program_name = "lesson2"
binary_data = bytes([
    0x06, 0x32,       # LD B, 50 (0x32 is 50)
    0x78,             # LD A, B
    0x32, 0x00, 0x70, # LD (0x7000), A
    0x76              # HALT
])

with open(f"programs/{program_name}.bin", "wb") as f:
    f.write(binary_data)

with open(f"programs/{program_name}.asm", "w") as f:
    f.write("; lesson2.asm - Memory Pokes\n")
    f.write("0000  06 32      LD B, 32h\n")
    f.write("0002  78         LD A, B\n")
    f.write("0003  32 00 70   LD (7000h), A\n")
    f.write("0006  76         HALT\n")

    print(f"Successfully generated {program_name}.bin and {program_name}.asm in /programs")