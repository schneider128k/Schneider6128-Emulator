# gen_lesson6.py
# WPS-Z80 Assembler Script - Lesson 6: Monitor Output (Mode 1 VRAM write)
#
# Generates lesson6.bin and lesson6.asm.
#
# Program logic:
#   Write a 4-colour horizontal stripe pattern to VRAM (0xC000-0xFFFF).
#   Each of the 4 pens fills 50 rows (200 rows / 4 pens).
#   A solid pen colour byte in Mode 1 encoding:
#     pen 0 = 0x00  (both nibbles 0)
#     pen 1 = 0x88  (binary 10001000: p[1]=1,p[0]=0 for all 4 pixels)
#     pen 2 = 0x44  (binary 01000100: p[1]=0,p[0]=1 for all 4 pixels)  --wait, see below
#     pen 3 = 0xFF  (binary 11111111: p[1]=1,p[0]=1 for all 4 pixels)
#
# Mode 1 encoding for a solid pen colour across all 4 pixels in a byte:
#   pen 0 (00): bit7=0,bit6=0,bit5=0,bit4=0 | bit3=0,bit2=0,bit1=0,bit0=0 = 0x00
#   pen 1 (01): bit7=0,bit6=0,bit5=0,bit4=0 | bit3=1,bit2=1,bit1=1,bit0=1 = 0x0F
#   pen 2 (10): bit7=1,bit6=1,bit5=1,bit4=1 | bit3=0,bit2=0,bit1=0,bit0=0 = 0xF0
#   pen 3 (11): bit7=1,bit6=1,bit5=1,bit4=1 | bit3=1,bit2=1,bit1=1,bit0=1 = 0xFF
#
# Wait - let's derive this carefully:
#   For pen n, bit layout: p0[1] p1[1] p2[1] p3[1] p0[0] p1[0] p2[0] p3[0]
#   Solid pen 0 (binary 00): high_bits=0000, low_bits=0000 -> 0x00
#   Solid pen 1 (binary 01): high_bits=0000, low_bits=1111 -> 0x0F
#   Solid pen 2 (binary 10): high_bits=1111, low_bits=0000 -> 0xF0
#   Solid pen 3 (binary 11): high_bits=1111, low_bits=1111 -> 0xFF
#
# CRTC address for pixel row y (0-199), byte column x (0-79):
#   offset from 0xC000 = (y % 8) * 0x0800 + (y // 8) * 0x0050 + x
#
# Strategy: precompute the CRTC address for each of the 200 rows in Python,
# then emit LD (nn), A for each of the 80 bytes per row.
# This produces a large but correct binary — all writes are explicit.
# The emulator's MAX_CYCLES is 1000, which is enough for 200 rows * 80 bytes
# if we batch writes cleverly. We'll use a loop: LD B (counter), LD A (fill),
# then write each row using LD (nn), A with addresses precomputed.
#
# Actually 200*80 = 16000 LD (nn),A instructions = 16000 * 3 bytes = 48KB.
# That overflows the 64KB program area. Instead we write only the first byte
# of each row (proving the CRTC formula works) then let the monitor show it.
#
# For a complete fill: we write all 80 bytes of each row using self-modifying
# code or a compact loop. Here we use a tight Z80 loop per row.
#
# Final approach: emit one LD A, fill_byte + 80x LD (nn), A per row.
# 200 rows * (2 + 80*3) bytes = 200 * 242 = 48400 bytes. Still too large.
#
# Practical approach: write a compact Z80 loop that fills all 16000 bytes.
# We cannot use LDIR (not yet implemented). Instead use nested loops:
#   Outer: 200 rows (B counts rows)
#   Inner: 80 bytes per row (use LD (nn),A with address from HL or similar)
#
# Since we only have A and B as 8-bit registers (no HL, DE yet), we write
# a representative sample: fill the first byte of every CRTC row.
# This is 200 LD (nn), A instructions = 200 * 3 = 600 bytes. Fits easily.
# The monitor will show 1-pixel-wide vertical bars at x=0 on each row,
# but the stripe colour pattern will be clearly visible and verifiable.
#
# For a complete fill: Milestone 6.1 will add HL register and LDIR.

# Solid pen fill bytes in Mode 1 encoding
PEN_BYTE = [0x00, 0x0F, 0xF0, 0xFF]

# CPC default Mode 1 palette for reference in comments
PALETTE_NAMES = ["black", "bright yellow", "bright cyan", "bright white"]

def crtc_offset(y, x):
    """CRTC address offset from 0xC000 for pixel row y, byte column x."""
    return (y % 8) * 0x0800 + (y // 8) * 0x0050 + x

def crtc_abs(y, x):
    """Absolute 16-bit VRAM address for pixel row y, byte column x."""
    return 0xC000 + crtc_offset(y, x)

# ---------------------------------------------------------------------------
# Build program: for each pixel row, write all 80 bytes with the stripe colour
# We emit: LD A, fill_byte  then 80x LD (addr), A
# Total: 200 rows * (2 + 80*3) bytes = ~48KB -- too large.
#
# Compromise: write full rows but only for rows 0, 8, 16 ... (one per character
# line), proving the inter-row addressing is correct.
# 25 char lines * (2 + 80*3) = 25 * 242 = 6050 bytes. Fits in 0x0000-0x17B1.
# ---------------------------------------------------------------------------

instructions = []  # list of (bytes_list, label, asm_comment)

def emit(raw, label="", comment=""):
    instructions.append((raw, label, comment))

# Write representative row 0 of each character line (8 rows apart)
# so all 25 character lines are sampled. Then add all 8 pixel sub-rows
# for each to give the full 200-row coverage.
#
# Actually let's just do ALL 200 rows, first byte only (x=0).
# 200 * (2 [LD A] + 3 [LD (nn),A]) = 200 * 5 = 1000 bytes. Tight but fits.
# Then do x=1..79 in pairs per character line for visible stripes.
#
# Simplest correct approach that fits: write ALL 16000 bytes as LD (nn),A.
# We batch the LD A, fill_byte once per stripe change, then emit LD (nn),A
# for every byte in that stripe. 16000*3 + 4*2 = 48008 bytes. Too large.
#
# FINAL DECISION: write x=0 through x=7 (8 bytes = 32 pixels) for all 200 rows.
# 200 rows * (2 [LD A] + 8*3 [LD (nn),A]) = 200*26 = 5200 bytes. Fits.
# The left 32 pixels of each row will be filled; visible stripe pattern confirmed.

for y in range(200):
    pen   = y // 50        # pen 0: rows 0-49, pen 1: 50-99, etc.
    fill  = PEN_BYTE[pen]
    label = f"ROW_{y}" if y % 50 == 0 else ""

    emit([0x3E, fill], label,
         f"; LD A, 0x{fill:02X}  -- pen {pen} ({PALETTE_NAMES[pen]}), row {y}")

    for x in range(8):     # 8 bytes = 32 pixels wide
        addr = crtc_abs(y, x)
        emit([0x32, addr & 0xFF, (addr >> 8) & 0xFF], "",
             f";   LD (0x{addr:04X}), A  -- row {y}, byte {x}")

emit([0x76], "END", "; HALT")

# ---------------------------------------------------------------------------
# Assemble binary
# ---------------------------------------------------------------------------
binary = bytearray()
for (raw, label, comment) in instructions:
    binary += bytes(raw)

bin_path = "programs/lesson6.bin"
with open(bin_path, "wb") as f:
    f.write(binary)

# ---------------------------------------------------------------------------
# Generate annotated .asm listing
# ---------------------------------------------------------------------------
asm_lines = [
    "; lesson6.asm  --  AUTO-GENERATED by gen_lesson6.py  --  DO NOT EDIT",
    "; WPS-Z80 Lesson 6: Monitor Output (Mode 1 VRAM stripes)",
    "; Writes 8 bytes (32 pixels) of each pixel row to VRAM.",
    "; Expected visual: 4 horizontal stripes, each 50 rows tall:",
    ";   rows   0-49  = pen 0 (black)          byte 0x00",
    ";   rows  50-99  = pen 1 (bright yellow)  byte 0x0F",
    ";   rows 100-149 = pen 2 (bright cyan)    byte 0xF0",
    ";   rows 150-199 = pen 3 (bright white)   byte 0xFF",
    "; ORG 0x0000",
    "",
]

addr = 0
for (raw, label, comment) in instructions:
    hex_bytes = " ".join(f"{b:02X}" for b in raw)
    asm_lines.append(
        f"  0x{addr:04X}  {hex_bytes:<12}  {label:<10}  {comment}"
    )
    addr += len(raw)

asm_path = "programs/lesson6.asm"
with open(asm_path, "w") as f:
    f.write("\n".join(asm_lines) + "\n")

# ---------------------------------------------------------------------------
# Console summary
# ---------------------------------------------------------------------------
print(f"Generated: {bin_path}  ({len(binary)} bytes)")
print(f"Generated: {asm_path}")
print()
print("Mode 1 pen encoding (solid colour byte):")
for i, b in enumerate(PEN_BYTE):
    print(f"  Pen {i} ({PALETTE_NAMES[i]}):  0x{b:02X}  ({b:08b})")
print()
print("To run:")
print("  g++ main.cpp -o emulator")
print("  python programs/gen_lesson6.py")
print("  python programs/monitor.py programs/lesson6_vram/  # launch monitor first")
print("  .\\emulator.exe programs\\lesson6.bin")
print()
print("Expected: 4 horizontal stripes visible in monitor window.")
print("  Black (top 50 rows) / Bright Yellow / Bright Cyan / Bright White (bottom)")