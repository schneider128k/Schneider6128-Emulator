# gen_lesson8.py
# WPS-Z80 Assembler Script - Lesson 8: Full ALU
# Generates lesson8.bin and lesson8.asm
#
# Program: Draw a filled rectangle using runtime address arithmetic.
#
# Phase 1 — Clear screen to pen 9 (green, 0x00 in Mode 0... wait)
#   Actually we need to think about Mode 0 encoding carefully.
#   Pen 9 = green = firmware colour 9.
#   Solid pen 9 byte: pen=9=0b1001, p[0]=1,p[1]=0,p[2]=0,p[3]=1
#   byte = p[0]<<7|p[0]<<6 | p[2]<<5|p[2]<<4 | p[1]<<3|p[1]<<2 | p[3]<<1|p[3]<<0
#        = 1<<7|1<<6 | 0<<5|0<<4 | 0<<3|0<<2 | 1<<1|1<<0
#        = 0b11000011 = 0xC3
#
# Phase 2 — Draw rectangle: rows 60-139 (80 rows tall), bytes 20-59 (40 bytes = 160px wide)
#   Pen 6 = bright red = 0b0110, p[0]=0,p[1]=1,p[2]=1,p[3]=0
#   byte = 0<<7|0<<6 | 1<<5|1<<4 | 1<<3|1<<2 | 0<<1|0<<0
#        = 0b00110000... wait let me recalculate:
#   p[0]=0: bit7=0, bit6=0
#   p[2]=1: bit5=1, bit4=1
#   p[1]=1: bit3=1, bit2=1
#   p[3]=0: bit1=0, bit0=0
#   byte = 0b00111100 = 0x3C
#
# CRTC address for row y, byte x:
#   addr = 0xC000 + (y%8)*0x0800 + (y//8)*0x0050 + x
#
# Strategy:
#   - Use LDIR to fill entire VRAM with pen 9 background (0xC3)
#   - Then loop over rectangle rows, compute CRTC address using ADD HL arithmetic,
#     use LDIR to fill each row's byte range with pen 6 (0x3C)
#
# Rectangle fill per row:
#   HL = CRTC address of (row y, byte 20)
#   BC = 40  (40 bytes to fill)
#   Fill 40 bytes with 0x3C using a write loop (no second LDIR source needed —
#   we use LD (HL),A / INC HL / DEC BC / JR NZ loop)
#
# Address computation for each rectangle row:
#   We iterate y from 60 to 139.
#   For each y: addr = 0xC000 + (y%8)*0x0800 + (y//8)*0x0050 + 20
#   We precompute these 80 addresses in the fill table at 0x0200.
#   Each entry is a 16-bit little-endian address = 2 bytes.
#   Table has 80 entries = 160 bytes.
#
# Main loop:
#   HL = 0x0200   (address table pointer)
#   B  = 80       (row count)
# ROW_LOOP:
#   E  = (HL)     (low byte of VRAM row address)
#   INC HL
#   D  = (HL)     (high byte of VRAM row address)
#   INC HL
#   ; DE now holds CRTC address of this rectangle row
#   LD A, 0x3C    (pen 6 fill byte)
#   LD C, 40      (bytes per rectangle row)
# FILL_LOOP:
#   LD (DE), A
#   INC DE
#   DEC C
#   JR NZ, FILL_LOOP
#   DEC B
#   JR NZ, ROW_LOOP
#   HALT

def solid_mode0_byte(pen):
    p = [(pen >> i) & 1 for i in range(4)]
    return (p[0]<<7)|(p[0]<<6)|(p[2]<<5)|(p[2]<<4)|(p[1]<<3)|(p[1]<<2)|(p[3]<<1)|p[3]

def crtc_abs(y, x):
    return 0xC000 + (y % 8) * 0x0800 + (y // 8) * 0x0050 + x

PEN_BG   = 9   # green background
PEN_RECT = 6   # bright red rectangle

BG_BYTE   = solid_mode0_byte(PEN_BG)
RECT_BYTE = solid_mode0_byte(PEN_RECT)

RECT_Y0   = 60
RECT_Y1   = 139
RECT_X0   = 20   # byte column (= pixel 40 in Mode 0)
RECT_W    = 40   # bytes wide  (= 80 pixels in Mode 0)

FILL_TABLE_ADDR = 0x0100   # 16384 bytes of background fill
ADDR_TABLE      = 0x4100   # 80 * 2 = 160 bytes of row addresses
CODE_START      = 0x4200   # program code

print(f"Background pen {PEN_BG}: byte = 0x{BG_BYTE:02X} ({BG_BYTE:08b})")
print(f"Rectangle pen {PEN_RECT}: byte = 0x{RECT_BYTE:02X} ({RECT_BYTE:08b})")

# ---------------------------------------------------------------------------
# Build background fill table (16384 bytes, all BG_BYTE)
# ---------------------------------------------------------------------------
fill_table = bytearray([BG_BYTE] * 16384)

# ---------------------------------------------------------------------------
# Build address table (80 rows * 2 bytes each)
# ---------------------------------------------------------------------------
addr_table = bytearray()
for y in range(RECT_Y0, RECT_Y1 + 1):
    addr = crtc_abs(y, RECT_X0)
    addr_table += bytes([addr & 0xFF, (addr >> 8) & 0xFF])

assert len(addr_table) == (RECT_Y1 - RECT_Y0 + 1) * 2

# ---------------------------------------------------------------------------
# Assemble program at CODE_START
# Phase 1: LDIR to fill background
# Phase 2: loop over address table, fill each rectangle row
# ---------------------------------------------------------------------------
code = bytearray()

def emit(b): code.extend(b)

# Phase 1: fill background
# LD HL, FILL_TABLE_ADDR
emit([0x21, FILL_TABLE_ADDR & 0xFF, (FILL_TABLE_ADDR >> 8) & 0xFF])
# LD DE, 0xC000
emit([0x11, 0x00, 0xC0])
# LD BC, 0x4000
emit([0x01, 0x00, 0x40])
# LDIR
emit([0xED, 0xB0])

# Phase 2: draw rectangle
ROW_COUNT = RECT_Y1 - RECT_Y0 + 1   # 80

# LD HL, ADDR_TABLE
emit([0x21, ADDR_TABLE & 0xFF, (ADDR_TABLE >> 8) & 0xFF])
# LD B, ROW_COUNT
emit([0x06, ROW_COUNT])

# ROW_LOOP:
row_loop_offset = len(code)   # byte offset within code block

# LD E, (HL)   — low byte of row address
emit([0x5E])
# INC HL
emit([0x23])
# LD D, (HL)   — high byte of row address
emit([0x56])
# INC HL
emit([0x23])
# LD A, RECT_BYTE
emit([0x3E, RECT_BYTE])
# LD C, RECT_W
emit([0x0E, RECT_W])

# FILL_LOOP:
fill_loop_offset = len(code)

# LD (DE), A
emit([0x12])
# INC DE
emit([0x13])
# DEC C
emit([0x0D])
# JR NZ, back to FILL_LOOP
fill_back = fill_loop_offset - (len(code) + 2)   # signed offset
emit([0x20, fill_back & 0xFF])

# DEC B
emit([0x05])
# JR NZ, back to ROW_LOOP
row_back = row_loop_offset - (len(code) + 2)
emit([0x20, row_back & 0xFF])

# HALT
emit([0x76])

# ---------------------------------------------------------------------------
# Assemble final binary
# Layout:
#   0x0000          : NOP sled / jump to CODE_START
#   0x0100          : fill_table (16384 bytes)
#   0x4100          : addr_table (160 bytes)
#   0x4200          : code
# ---------------------------------------------------------------------------
BINARY_SIZE = CODE_START + len(code)
program = bytearray(BINARY_SIZE)

# Jump from 0x0000 to CODE_START
program[0x0000] = 0xC3                          # JP nn
program[0x0001] = CODE_START & 0xFF
program[0x0002] = (CODE_START >> 8) & 0xFF

# Fill table
program[FILL_TABLE_ADDR:FILL_TABLE_ADDR + 16384] = fill_table

# Address table
program[ADDR_TABLE:ADDR_TABLE + len(addr_table)] = addr_table

# Code
program[CODE_START:CODE_START + len(code)] = code

# ---------------------------------------------------------------------------
# Write binary
# ---------------------------------------------------------------------------
bin_path = "programs/lesson8.bin"
with open(bin_path, "wb") as f:
    f.write(program)

# ---------------------------------------------------------------------------
# Write .asm listing
# ---------------------------------------------------------------------------
asm_lines = [
    "; lesson8.asm  --  AUTO-GENERATED by gen_lesson8.py  --  DO NOT EDIT",
    "; WPS-Z80 Lesson 8: Full ALU",
    "; Draws a filled rectangle using runtime address arithmetic.",
    ";",
    f"; Background: pen {PEN_BG} (green)    fill byte = 0x{BG_BYTE:02X}",
    f"; Rectangle:  pen {PEN_RECT} (bright red) fill byte = 0x{RECT_BYTE:02X}",
    f"; Rectangle:  rows {RECT_Y0}-{RECT_Y1}, byte columns {RECT_X0}-{RECT_X0+RECT_W-1}",
    f";             = pixel columns {RECT_X0*2}-{(RECT_X0+RECT_W)*2-1} in Mode 0",
    ";",
    "; ORG 0x0000",
    f"  0x0000  C3 00 42          JP 0x{CODE_START:04X}     ; jump to code",
    f"  0x{FILL_TABLE_ADDR:04X}  [16384 bytes]     fill_table  ; background fill (pen {PEN_BG})",
    f"  0x{ADDR_TABLE:04X}  [160 bytes]       addr_table  ; CRTC row addresses for rectangle",
    "",
    f"; CODE at 0x{CODE_START:04X}",
    f"  0x{CODE_START:04X}  21 00 01          LD HL, 0x{FILL_TABLE_ADDR:04X}  ; phase 1: fill background",
    f"  ...     11 00 C0          LD DE, 0xC000",
    f"  ...     01 00 40          LD BC, 0x4000",
    f"  ...     ED B0             LDIR",
    "",
    f"  ...     21 00 41          LD HL, 0x{ADDR_TABLE:04X}  ; phase 2: draw rectangle",
    f"  ...     06 {ROW_COUNT:02X}              LD B, {ROW_COUNT}         ; row counter",
    ";",
    "; ROW_LOOP:",
    "  ...     5E                LD E, (HL)      ; low byte of row CRTC address",
    "  ...     23                INC HL",
    "  ...     56                LD D, (HL)      ; high byte of row CRTC address",
    "  ...     23                INC HL",
    f"  ...     3E {RECT_BYTE:02X}              LD A, 0x{RECT_BYTE:02X}       ; pen {PEN_RECT} fill byte",
    f"  ...     0E {RECT_W:02X}              LD C, {RECT_W}          ; bytes per row",
    ";",
    "; FILL_LOOP:",
    "  ...     12                LD (DE), A      ; write pixel byte",
    "  ...     13                INC DE          ; advance VRAM pointer",
    "  ...     0D                DEC C           ; decrement byte counter",
    "  ...     20 FA             JR NZ, FILL_LOOP",
    "  ...     05                DEC B           ; decrement row counter",
    "  ...     20 F2             JR NZ, ROW_LOOP",
    "  ...     76                HALT",
]

asm_path = "programs/lesson8.asm"
with open(asm_path, "w") as f:
    f.write("\n".join(asm_lines) + "\n")

# ---------------------------------------------------------------------------
# Console summary
# ---------------------------------------------------------------------------
print(f"\nGenerated: {bin_path}  ({len(program)} bytes)")
print(f"Generated: {asm_path}")
print()
print(f"Fill table:  0x{FILL_TABLE_ADDR:04X}  ({16384} bytes, pen {PEN_BG} = 0x{BG_BYTE:02X})")
print(f"Addr table:  0x{ADDR_TABLE:04X}  ({len(addr_table)} bytes, {ROW_COUNT} row addresses)")
print(f"Code:        0x{CODE_START:04X}  ({len(code)} bytes)")
print()
print("To run:")
print("  g++ main.cpp -o emulator.exe -std=c++17")
print("  python programs/gen_lesson8.py")
print(f"  .\\monitor.exe programs\\lesson8_vram --mode 0   (terminal 1)")
print(f"  .\\emulator.exe programs\\lesson8.bin             (terminal 2)")
print()
print("Expected: green screen with a bright red rectangle in the centre.")
print(f"  Rectangle: {RECT_W*2} pixels wide x {ROW_COUNT} rows tall")
print(f"  Position:  pixel column {RECT_X0*2}, row {RECT_Y0}")