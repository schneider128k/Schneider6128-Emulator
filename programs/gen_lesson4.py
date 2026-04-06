import os

def generate_lesson4():
    if not os.path.exists('programs'):
        os.makedirs('programs')

    # 1. Define the Machine Code (The Binary)
    # ---------------------------------------------------------
    program_bytes = bytearray([
        0x3E, 0x0A,    # LD A, 10
        0xFE, 0x0A,    # CP 10      (Sets Z flag)
        0x06, 0x05,    # LD B, 5
        0x05,           # DEC B      (Resets Z flag)
        0x3E, 0x02,    # LD A, 2
        0xFE, 0x05,    # CP 5       (Sets C flag)
        0x76            # HALT
    ])

    # 2. Define the Assembly Source (The Documentation)
    # ---------------------------------------------------------
    asm_source = """; WPS-Z80 LESSON 4: LOGIC & FLAGS
; Generated for Milestone 4 Testing
; -----------------------------------------
ORG 0000h

START:
    LD A, 0Ah       ; Load 10 into Accumulator
    CP 0Ah          ; Compare A with 10 (Expect Z=1)
    
    LD B, 05h       ; Load 5 into B
    DEC B           ; Decrement B (4 != 0, Expect Z=0)
    
    LD A, 02h       ; Load 2 into A
    CP 05h          ; Compare A with 5 (2 < 5, Expect C=1)
    
    HALT            ; End of Test
"""

    # Write the Binary File
    with open('programs/lesson4.bin', 'wb') as f:
        f.write(program_bytes)

    # Write the Assembly File
    with open('programs/lesson4.asm', 'w') as f:
        f.write(asm_source)

    print("WPS Build Successful:")
    print("  [BIN] programs/lesson4.bin")
    print("  [ASM] programs/lesson4.asm")

if __name__ == "__main__":
    generate_lesson4()