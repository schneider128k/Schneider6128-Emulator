# programs/gen_lesson1.py
program_name = "lesson1"
binary_data = bytes([
    0x3E, 0x0A,       # LD A, 10
    0x3C,             # INC A
    0x3C,             # INC A
    0xC3, 0x00, 0x00  # JP 0x0000 (Infinite Loop)
])

with open(f"programs/{program_name}.bin", "wb") as f:
    f.write(binary_data)

with open(f"programs/{program_name}.asm", "w") as f:
    f.write("; lesson1.asm - Basic Arithmetic & Jumps\n")
    f.write("ORG 0000h\n\n")
    f.write("0000  3E 0A      LD A, 0Ah\n")
    f.write("0002  3C         INC A\n")
    f.write("0003  3C         INC A\n")
    f.write("0004  C3 00 00   JP 0000h\n")

    print(f"Successfully generated {program_name}.bin and {program_name}.asm in /programs")