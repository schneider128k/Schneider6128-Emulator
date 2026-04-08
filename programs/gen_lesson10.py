# gen_lesson10.py
# WPS-Z80 Assembler Script — Lesson 10: Movement
# Generates lesson10.bin and lesson10.asm
#
# Scene (48 frames):
#   - Dark blue background (pen 1), yellow-brown ground at y=148
#   - Alligator starts at byte col 60, moves LEFT  1 byte/frame (facing left)
#   - Hero      starts at byte col  8, moves RIGHT 1 byte/frame (facing right)
#   - Walk cycle: alternates A/B frame every 4 display frames
#   - After 26 frames they pass through each other near the centre
#
# New Z80 concepts introduced:
#   DJNZ e        — outer frame loop counter (DEC B + JR NZ in one opcode)
#   OUT (n), A    — frame-sync hook: signals emulator to dump VRAM immediately
#
# Architecture for runtime positions:
#   Sprite data tables hold ONLY (mask, sprite_byte) pairs — no VRAM addresses.
#   A separate row-address table holds the base VRAM address for each sprite
#   row (computed once from fixed Y, stored as data).  At blit time the Z80
#   reads the base row address, adds the current X byte-column, and uses the
#   result as the VRAM destination.  This separates Y (fixed) from X (moving).
#
# RAM layout (scratch variables at 0x8000):
#   0x8000  alig_x    (1 byte) alligator byte-column (decrements each frame)
#   0x8001  hero_x    (1 byte) hero byte-column      (increments each frame)
#   0x8002  frame_ctr (1 byte) animation frame toggle (bit 0 selects A vs B)
#
# Memory layout:
#   0x0000  JP CODE_START   (3 bytes)
#   0x0100  BG fill table   (16384 bytes, solid pen 1) — used by initial LDIR
#   0x4000  Alligator row-address table (32 bytes: 16 rows × 2 bytes each)
#   0x4020  Hero      row-address table (40 bytes: 20 rows × 2 bytes each)
#   0x4050  Alligator sprite data — frame A closed (16 rows × 24 bytes = 384 bytes)
#   0x41D0  Alligator sprite data — frame B closed (384 bytes)
#   0x4350  Hero sprite data      — run A          (20 rows × 16 bytes = 320 bytes)
#   0x4490  Hero sprite data      — run B          (320 bytes)
#   0x45D0  Ground table          (4 rows × 82 bytes = 328 bytes)
#   0x4720  Z80 code
#   0x8000  RAM scratch

import struct

# ---------------------------------------------------------------------------
# Encoding helpers  (identical to lesson 9)
# ---------------------------------------------------------------------------

def solid_mode0_byte(pen):
    p = [(pen >> i) & 1 for i in range(4)]
    return (p[0]<<7)|(p[0]<<6)|(p[2]<<5)|(p[2]<<4)|(p[1]<<3)|(p[1]<<2)|(p[3]<<1)|p[3]

def two_mode0_pixels(pl, pr):
    l = [(pl >> i) & 1 for i in range(4)]
    r = [(pr >> i) & 1 for i in range(4)]
    return (l[0]<<7)|(r[0]<<6)|(l[2]<<5)|(r[2]<<4)|(l[1]<<3)|(r[1]<<2)|(l[3]<<1)|r[3]

def mode0_mask(pl, pr, tp=0):
    lt = (pl == tp); rt = (pr == tp)
    mask = 0
    if lt: mask |= (1<<7)|(1<<5)|(1<<3)|(1<<1)
    if rt: mask |= (1<<6)|(1<<4)|(1<<2)|(1<<0)
    return mask

def crtc_abs(y, x):
    return 0xC000 + (y % 8) * 0x0800 + (y // 8) * 0x0050 + x

def mirror_sprite(grid):
    return [row[::-1] for row in grid]

def encode_sprite_data(pixel_grid, tp=0):
    """
    Encode a sprite pixel grid into a flat list of (mask, sprite_byte) pairs.
    No VRAM addresses embedded — those are handled at runtime from the
    row-address table plus the current x byte-column.
    Returns a bytearray of length (rows * bytes_per_row * 2).
    """
    data = bytearray()
    for row in pixel_grid:
        assert len(row) % 2 == 0
        for x in range(0, len(row), 2):
            pl, pr = row[x], row[x+1]
            data += bytes([mode0_mask(pl, pr, tp), two_mode0_pixels(pl, pr)])
    return data

def make_row_addr_table(sprite_y, sprite_h):
    """
    Build a table of base VRAM addresses for each row of a sprite,
    assuming x=0 (the x byte-column is added at runtime).
    Returns a bytearray of length sprite_h * 2 (lo, hi per row).
    """
    data = bytearray()
    for r in range(sprite_h):
        addr = crtc_abs(sprite_y + r, 0)
        data += bytes([addr & 0xFF, (addr >> 8) & 0xFF])
    return data

# ---------------------------------------------------------------------------
# Pen constants
# ---------------------------------------------------------------------------
BK = 0;  G1 = 9;  G2 = 15; WH = 13; YL = 12; RD = 6;  PK = 7
HAIR=3;  SKIN=8;  SHIRT=6; TROU=2;  SHOE=13
H=HAIR; K=SKIN; O=SHIRT; N=TROU; S=SHOE; _=BK

# ---------------------------------------------------------------------------
# Alligator pixel grids — facing RIGHT, then mirrored to LEFT
# 24 px wide × 16 rows tall = 12 bytes × 16 rows
# ---------------------------------------------------------------------------

_ALIG_CLOSED_A = [
      [ _,  _,  _,  _,  G2, _,  G2, _,  G2, _,  G2, _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _],
      [ _,  _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, _,  _,  _,  _,  _,  _,  _,  _],
      [ _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, _,  _],
      [ _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, YL, G1, G1, _],
      [ _,  G1, G1, G1, WH, WH, WH, WH, WH, WH, WH, WH, G1, G1, G1, G1, G1, G1, G1, G1, YL, G1, G1, _],
      [ _,  G1, G1, G1, WH, WH, WH, WH, WH, WH, WH, WH, G1, G1, G1, G1, G1, G1, G1, G1, _,  G1, G1, _],
      [ _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, WH, WH, WH, WH, WH, WH, WH, WH, G1],
      [ _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, WH, WH, WH, WH, WH, WH, WH, WH, G1],
      [ _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, WH, WH, WH, WH, WH, WH, WH, WH, G1],
      [ _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, WH, WH, WH, WH, WH, WH, WH, WH, G1],
      [ _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, _],
      [ _,  _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, _,  _],
      [ _,  _,  _,  G1, _,  _,  _,  G1, _,  _,  _,  _,  _,  _,  _,  _,  G1, _,  _,  _,  G1, _,  _,  _],
      [ _,  _,  _,  G1, _,  _,  _,  G1, _,  _,  _,  _,  _,  _,  _,  _,  G1, _,  _,  _,  G1, _,  _,  _],
      [ _,  _,  G1, G1, G1, _,  _,  G1, G1, G1, _,  _,  _,  _,  _,  G1, G1, G1, _,  _,  G1, G1, G1, _],
      [ _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _],
]

_ALIG_CLOSED_B = [
      [ _,  _,  _,  _,  G2, _,  G2, _,  G2, _,  G2, _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _],
      [ _,  _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, _,  _,  _,  _,  _,  _,  _,  _],
      [ _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, _,  _],
      [ _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, YL, G1, G1, _],
      [ _,  G1, G1, G1, WH, WH, WH, WH, WH, WH, WH, WH, G1, G1, G1, G1, G1, G1, G1, G1, YL, G1, G1, _],
      [ _,  G1, G1, G1, WH, WH, WH, WH, WH, WH, WH, WH, G1, G1, G1, G1, G1, G1, G1, G1, _,  G1, G1, _],
      [ _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, WH, WH, WH, WH, WH, WH, WH, WH, G1],
      [ _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, WH, WH, WH, WH, WH, WH, WH, WH, G1],
      [ _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, WH, WH, WH, WH, WH, WH, WH, WH, G1],
      [ _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, WH, WH, WH, WH, WH, WH, WH, WH, G1],
      [ _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, _],
      [ _,  _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, _,  _],
      [ _,  _,  G1, _,  _,  _,  G1, _,  _,  _,  _,  _,  _,  _,  _,  G1, _,  _,  _,  _,  G1, _,  _,  _],
      [ _,  _,  G1, _,  _,  _,  G1, _,  _,  _,  _,  _,  _,  _,  _,  G1, _,  _,  _,  _,  G1, _,  _,  _],
      [ _,  G1, G1, G1, _,  _,  G1, G1, G1, _,  _,  _,  _,  _,  G1, G1, G1, _,  _,  _,  G1, G1, G1, _],
      [ _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _],
]

ALIG_CLOSED_A = mirror_sprite(_ALIG_CLOSED_A)
ALIG_CLOSED_B = mirror_sprite(_ALIG_CLOSED_B)

# ---------------------------------------------------------------------------
# Hero pixel grids — natively facing RIGHT
# 16 px wide × 20 rows tall = 8 bytes × 20 rows
# ---------------------------------------------------------------------------

HERO_RUN_A = [
      [ _,  _,  _,  H,  H,  H,  H,  H,  H,  H,  H,  H,  H,  _,  _,  _],
      [ _,  _,  H,  H,  H,  H,  H,  H,  H,  H,  H,  H,  H,  H,  _,  _],
      [ _,  _,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  _,  _],
      [ _,  _,  K,  K,  K,  K,  K,  _,  K,  K,  K,  K,  _,  K,  _,  _],
      [ _,  _,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  _,  _],
      [ _,  _,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  _,  _],
      [ _,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  _],
      [ O,  O,  O,  O,  K,  K,  K,  K,  K,  K,  K,  K,  O,  O,  O,  O],
      [ O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O],
      [ O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O],
      [ _,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  _],
      [ _,  _,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  _,  _],
      [ _,  _,  N,  N,  N,  N,  _,  _,  _,  _,  N,  N,  N,  N,  _,  _],
      [ _,  _,  N,  N,  N,  N,  _,  _,  _,  _,  N,  N,  N,  N,  _,  _],
      [ _,  _,  N,  N,  N,  N,  _,  _,  _,  _,  N,  N,  N,  N,  _,  _],
      [ _,  _,  _,  N,  N,  N,  _,  _,  N,  N,  N,  N,  _,  _,  _,  _],
      [ _,  _,  _,  N,  N,  N,  _,  _,  N,  N,  N,  N,  _,  _,  _,  _],
      [ _,  _,  _,  N,  N,  N,  _,  _,  _,  N,  N,  N,  _,  _,  _,  _],
      [ _,  _,  S,  S,  S,  S,  _,  _,  _,  _,  S,  S,  S,  S,  _,  _],
      [ _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _],
]

HERO_RUN_B = [
      [ _,  _,  _,  H,  H,  H,  H,  H,  H,  H,  H,  H,  H,  _,  _,  _],
      [ _,  _,  H,  H,  H,  H,  H,  H,  H,  H,  H,  H,  H,  H,  _,  _],
      [ _,  _,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  _,  _],
      [ _,  _,  K,  K,  K,  K,  K,  _,  K,  K,  K,  K,  _,  K,  _,  _],
      [ _,  _,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  _,  _],
      [ _,  _,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  _,  _],
      [ _,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  _],
      [ O,  O,  O,  O,  K,  K,  K,  K,  K,  K,  K,  K,  O,  O,  O,  O],
      [ O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O],
      [ O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O],
      [ _,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  _],
      [ _,  _,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  _,  _],
      [ _,  _,  _,  N,  N,  N,  _,  _,  N,  N,  N,  N,  _,  _,  _,  _],
      [ _,  _,  _,  N,  N,  N,  _,  _,  N,  N,  N,  N,  _,  _,  _,  _],
      [ _,  _,  _,  N,  N,  N,  _,  _,  N,  N,  N,  N,  _,  _,  _,  _],
      [ _,  _,  _,  _,  N,  N,  N,  N,  N,  N,  _,  _,  _,  _,  _,  _],
      [ _,  _,  _,  _,  N,  N,  N,  N,  N,  N,  _,  _,  _,  _,  _,  _],
      [ _,  _,  _,  _,  N,  N,  N,  N,  N,  N,  _,  _,  _,  _,  _,  _],
      [ _,  _,  _,  _,  S,  S,  S,  S,  S,  S,  _,  _,  _,  _,  _,  _],
      [ _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _],
]

# ---------------------------------------------------------------------------
# Validate sprite dimensions
# ---------------------------------------------------------------------------
for name, grid, ew, eh in [
    ("ALIG_CLOSED_A", ALIG_CLOSED_A, 24, 16),
    ("ALIG_CLOSED_B", ALIG_CLOSED_B, 24, 16),
    ("HERO_RUN_A",    HERO_RUN_A,    16, 20),
    ("HERO_RUN_B",    HERO_RUN_B,    16, 20),
]:
    assert len(grid) == eh, f"{name}: expected {eh} rows, got {len(grid)}"
    for i, row in enumerate(grid):
        assert len(row) == ew, f"{name} row {i}: expected {ew} px, got {len(row)}"
print("All sprite dimensions validated.")

# ---------------------------------------------------------------------------
# Scene constants
# ---------------------------------------------------------------------------
BG_PEN      = 1
BG_BYTE     = solid_mode0_byte(BG_PEN)
GROUND_PEN  = 12
GROUND_BYTE = solid_mode0_byte(GROUND_PEN)

ALIG_W      = 24;  ALIG_BYTES = ALIG_W // 2   # 12 bytes/row
ALIG_H      = 16
HERO_W      = 16;  HERO_BYTES = HERO_W // 2   # 8  bytes/row
HERO_H      = 20
GROUND_Y    = 148; GROUND_H   = 4; GROUND_W   = 80

ALIG_Y      = GROUND_Y - ALIG_H   # 132
HERO_Y      = GROUND_Y - HERO_H   # 128

ALIG_X_START = 60   # byte col — alligator starts right, moves left
HERO_X_START = 8    # byte col — hero starts left, moves right
NUM_FRAMES   = 48

# Clamp bounds (so sprites never partially leave screen)
ALIG_X_MIN   = 0
ALIG_X_MAX   = 80 - ALIG_BYTES   # 68
HERO_X_MIN   = 0
HERO_X_MAX   = 80 - HERO_BYTES   # 72

# ---------------------------------------------------------------------------
# Build data tables
# ---------------------------------------------------------------------------

# Row-address tables (base addresses at x=0; x added at runtime)
alig_row_addrs = make_row_addr_table(ALIG_Y, ALIG_H)
hero_row_addrs = make_row_addr_table(HERO_Y, HERO_H)

# Sprite data (mask, spr_byte pairs — no addresses)
# Each frame:  rows × bytes_per_row × 2 bytes
alig_data_A = encode_sprite_data(ALIG_CLOSED_A)   # 16 × 12 × 2 = 384 bytes
alig_data_B = encode_sprite_data(ALIG_CLOSED_B)   # 384 bytes
hero_data_A = encode_sprite_data(HERO_RUN_A)      # 20 × 8  × 2 = 320 bytes
hero_data_B = encode_sprite_data(HERO_RUN_B)      # 320 bytes

# Ground table: [dest_lo, dest_hi, BG_BYTE*80] per row (no masking)
ground_addrs = [crtc_abs(GROUND_Y + r, 0) for r in range(GROUND_H)]
ground_table = bytearray()
for addr in ground_addrs:
    ground_table += bytes([addr & 0xFF, (addr >> 8) & 0xFF])
    ground_table += bytes([GROUND_BYTE] * GROUND_W)

# Background fill table (16384 bytes)
bg_fill = bytes([BG_BYTE] * 16384)

# ---------------------------------------------------------------------------
# Memory layout addresses
# ---------------------------------------------------------------------------
FILL_TABLE        = 0x6000   # SAFE: after all sprite data and code
ALIG_ROW_ADDRS    = 0x4000   # 32 bytes
HERO_ROW_ADDRS    = 0x4020   # 40 bytes
ALIG_DATA_A       = 0x4050   # 384 bytes  → ends at 0x41D0
ALIG_DATA_B       = 0x41D0   # 384 bytes  → ends at 0x4350
HERO_DATA_A       = 0x4350   # 320 bytes  → ends at 0x4490
HERO_DATA_B       = 0x4490   # 320 bytes  → ends at 0x45D0
GROUND_TABLE      = 0x45D0   # 328 bytes  → ends at 0x4718
CODE_START        = 0x4720

ALIG_X_ADDR       = 0xB000
HERO_X_ADDR       = 0xB001
FRAME_CTR_ADDR    = 0xB002

# Sanity checks
assert ALIG_ROW_ADDRS + len(alig_row_addrs) <= HERO_ROW_ADDRS
assert HERO_ROW_ADDRS + len(hero_row_addrs) <= ALIG_DATA_A
assert ALIG_DATA_A    + len(alig_data_A)    <= ALIG_DATA_B
assert ALIG_DATA_B    + len(alig_data_B)    <= HERO_DATA_A
assert HERO_DATA_A    + len(hero_data_A)    <= HERO_DATA_B
assert HERO_DATA_B    + len(hero_data_B)    <= GROUND_TABLE
assert GROUND_TABLE   + len(ground_table)   <= CODE_START

print(f"alig_row_addrs:  {len(alig_row_addrs)} bytes at 0x{ALIG_ROW_ADDRS:04X}")
print(f"hero_row_addrs:  {len(hero_row_addrs)} bytes at 0x{HERO_ROW_ADDRS:04X}")
print(f"alig_data_A:     {len(alig_data_A)} bytes at 0x{ALIG_DATA_A:04X}")
print(f"alig_data_B:     {len(alig_data_B)} bytes at 0x{ALIG_DATA_B:04X}")
print(f"hero_data_A:     {len(hero_data_A)} bytes at 0x{HERO_DATA_A:04X}")
print(f"hero_data_B:     {len(hero_data_B)} bytes at 0x{HERO_DATA_B:04X}")
print(f"ground_table:    {len(ground_table)} bytes at 0x{GROUND_TABLE:04X}")
print(f"code starts at:  0x{CODE_START:04X}")

BINARY_SIZE = 0xA100   # fill table ends at 0x6000+0x4000=0xA000; add a little margin
program = bytearray(BINARY_SIZE)

# JP to CODE_START at reset vector
program[0x0000] = 0xC3
program[0x0001] = CODE_START & 0xFF
program[0x0002] = (CODE_START >> 8) & 0xFF

# Embed data tables
for i, b in enumerate(bg_fill):          program[FILL_TABLE     + i] = b
for i, b in enumerate(alig_row_addrs):   program[ALIG_ROW_ADDRS + i] = b
for i, b in enumerate(hero_row_addrs):   program[HERO_ROW_ADDRS + i] = b
for i, b in enumerate(alig_data_A):      program[ALIG_DATA_A    + i] = b
for i, b in enumerate(alig_data_B):      program[ALIG_DATA_B    + i] = b
for i, b in enumerate(hero_data_A):      program[HERO_DATA_A    + i] = b
for i, b in enumerate(hero_data_B):      program[HERO_DATA_B    + i] = b
for i, b in enumerate(ground_table):     program[GROUND_TABLE   + i] = b

# ---------------------------------------------------------------------------
# Z80 Code assembly
#
# All addresses in `code` are relative to CODE_START.
# Helper: abs_addr(offset) = CODE_START + offset
# ---------------------------------------------------------------------------
code = bytearray()
asm  = []   # ASM listing lines

def emit(b, comment=""):
    code.extend(b)
    if comment:
        asm.append(f"  ; {comment}")

def abs_addr(offset):
    return CODE_START + offset

# --- Low-level emitters for forward/backward jumps ---
# For forward jumps we patch the displacement after we know the target.
# For backward jumps we know the target already.

def jr_back(target_offset):
    """Emit JR NZ, <backward> — displacement from end of JR instruction to target."""
    here = len(code) + 2          # PC after consuming the 2-byte JR
    disp = target_offset - here   # negative
    assert -128 <= disp < 0, f"JR backward displacement out of range: {disp}"
    return bytes([0x20, disp & 0xFF])

def djnz_back(target_offset):
    """Emit DJNZ <backward>."""
    here = len(code) + 2
    disp = target_offset - here
    assert -128 <= disp < 0, f"DJNZ displacement out of range: {disp}"
    return bytes([0x10, disp & 0xFF])

# ---------------------------------------------------------------------------
# Subroutine: MASKED_BLIT
#
# Blits one sprite onto VRAM using AND-mask transparency.
#
# Entry conditions:
#   IX  — pointer to row-address table  [lo, hi] per row
#         (base address at x=0 for each row)
#   IY  — pointer to sprite data table  [mask, spr_byte] per byte-pair
#   B   — number of rows
#   C   — number of byte-pairs per row  (= sprite_width_bytes)
#   D   — x byte-column to add to each row address
#
# We don't have IX/IY yet — use HL for the row-address table and
# push/pop to save it across inner loops, passing sprite data in a
# separate register pair.
#
# Revised register allocation (no IX/IY needed):
#   HL  — row-address table pointer   (advances 2 bytes per row)
#   DE  — VRAM destination (computed per row: base_addr + x_col)
#   IX_SHADOW via stack: sprite data pointer lives on the stack between rows
#
# Simplest approach without IX/IY: use a fixed RAM pointer for the
# sprite data base, advance it row by row.
#   0x8003 / 0x8004: sprite data current pointer (lo, hi)
#
# Per-row loop:
#   1. Read [dest_lo, dest_hi] from HL → DE_base
#   2. Add x_col (from RAM) to DE_base → DE = VRAM dest for this row
#   3. Load sprite data pointer from RAM into HL2 (we need two HL's — use stack)
#   4. Inner byte loop (C iterations):
#        LD B_inner, (HL2)  INC HL2   ; mask
#        LD A, (HL2)        INC HL2   ; sprite byte
#        PUSH HL2
#        LD L, A
#        LD A, (DE)
#        AND B_inner
#        OR  L
#        LD  (DE), A
#        POP HL2
#        INC DE
#        DEC C_inner / JR NZ
#   5. Save updated HL2 back to RAM
#   6. Restore HL (row-addr ptr) and advance outer row counter
#
# Register pressure is tight. We use:
#   HL  = row-address table pointer
#   DE  = VRAM destination
#   BC  = outer: B=row_count (decremented by DJNZ), C=bytes_per_row (reloaded each row)
#         inner: B=mask, C=byte_counter
#   A   = scratch
#   RAM 0x8003/0x8004 = sprite data pointer
#   RAM 0x8005        = bytes_per_row constant (saved/restored for inner loop)
#
# This is the standard CPC approach: RAM as extra registers.
# ---------------------------------------------------------------------------

SPR_PTR_LO   = 0xB003
SPR_PTR_HI   = 0xB004
BYTES_PR_ROW = 0x8005   # bytes_per_row constant saved here for inner loop reload

def emit_subroutine_masked_blit():
    """
    Emit the MASKED_BLIT subroutine.
    Called with CALL; returns with RET.

    Caller sets up before CALL:
      HL  = row-address table pointer
      A   = x byte-column
      B   = row count
      C   = bytes_per_row
      (0xB003/0xB004) = sprite data pointer (set by caller)
    """
    global asm
    sub_offset = len(code)
    asm.append(f"\nMASKED_BLIT:  ; addr 0x{abs_addr(sub_offset):04X}")

    # Save bytes_per_row into RAM (we need C for mask in inner loop)
    # LD A, C  /  LD (BYTES_PR_ROW), A
    emit([0x79])                                                          # LD A, C
    emit([0x32, BYTES_PR_ROW & 0xFF, (BYTES_PR_ROW >> 8) & 0xFF])        # LD (BYTES_PR_ROW), A

    row_loop = len(code)
    asm.append(f"  .row_loop:   ; offset {row_loop}")

    # Read base row address from row-addr table into DE
    emit([0x5E, 0x23])   # LD E,(HL) / INC HL
    emit([0x56, 0x23])   # LD D,(HL) / INC HL

    # DE = base_addr + x_col
    # Load x_col from RAM into A, add to E, handle carry into D
    # We have DE = base_addr. We want DE += x_col.
    # Use: LD A, E / ADD A, x_col / LD E, A / JR NC, no_carry / INC D
    # x_col is passed in via scratch RAM — caller wrote it to A before CALL,
    # but we consumed A above for bytes_per_row. So caller writes x_col to
    # a dedicated RAM cell instead.
    # RAM 0x8006 = x_col for current sprite (set by caller)
    X_COL_SCRATCH = 0xB006
    emit([0x3A, X_COL_SCRATCH & 0xFF, (X_COL_SCRATCH >> 8) & 0xFF])  # LD A, (X_COL)
    emit([0x7B])                                                        # LD A, E  (oops — overwrites)
    # Correction: we need to add x_col to E without losing x_col.
    # Load E into A, add x_col (now in scratch), handle carry.
    # But we just loaded A from scratch then immediately overwrote with LD A,E.
    # Reorder: LD A, E first, then ADD A, (indirect x_col).
    # We can't ADD A,(nn) directly on Z80. Use:
    #   LD A, E
    #   LD C, (X_COL_SCRATCH)   -- but C is bytes_per_row...
    # Better: use the already-saved BYTES_PR_ROW and save x_col differently.
    # Cleanest: dedicate a register to x_col for the whole subroutine.
    # Since B is the row counter (DJNZ), C is bytes_per_row (reloaded),
    # D and E are VRAM dest, H and L are row-addr table pointer —
    # there are NO free registers. This is the classic Z80 dilemma.
    #
    # Solution: PUSH BC before inner loop, use C for byte counter,
    # restore BC (row count in B) after inner loop. x_col lives in RAM.
    # Erase the last few emits and restart this section cleanly.
    pass

    return sub_offset


# The register pressure analysis above shows we need a cleaner strategy.
# Let's step back and use the simplest correct approach:
#
# STRATEGY: Inline blit (no subroutine), unrolled per sprite.
# For M10 this is fine — we have two sprites, each called twice per frame
# (erase then blit). We emit 4 inline blit blocks per frame.
# A subroutine with CALL/RET comes in M11 when the code grows further.
#
# Inline blit register layout:
#   HL  = row-address table (advances 2/row)
#   DE  = VRAM destination  (computed each row)
#   B   = row counter       (DJNZ outer loop)
#   C   = byte counter      (reloaded each row from RAM)
#   0x8003/0x8004 = sprite data ptr (lo/hi), updated each row
#   0x8005 = bytes_per_row constant
#   0x8006 = x_col for current blit

# Clear the incorrect partial subroutine attempt
code.clear()
asm.clear()

SPR_PTR_LO   = 0xB003
SPR_PTR_HI   = 0xB004
BYTES_PR_SAVE= 0xB005
X_COL_SCRATCH= 0xB006

def emit_blit_block(row_addr_table, spr_data_addr,
                    row_count, bytes_per_row, x_col_addr,
                    label, is_erase=False):
    """
    Emit an inline masked blit or erase block.

    For a normal blit:
      Reads (mask, spr_byte) pairs from sprite data table.
      For each VRAM byte: A = (A_bg AND mask) OR spr_byte.

    For an erase (is_erase=True):
      Ignores sprite data; writes BG_BYTE unconditionally.
      This is fast: just LD A, BG_BYTE / LD (DE), A / INC DE per byte.

    Register use:
      HL  = row-address table pointer
      DE  = VRAM destination (base_row_addr + x_col, computed per row)
      B   = row counter (DJNZ)
      C   = byte counter (inner loop, reloaded each row)
      0xB003/0xB004 = current sprite data pointer (blit only)
    """
    global asm
    asm.append(f"\n  ; --- {label} ---")

    # Initialise sprite data pointer in RAM (blit only)
    if not is_erase:
        emit([0x3E, spr_data_addr & 0xFF])                                   # LD A, lo
        emit([0x32, SPR_PTR_LO & 0xFF, (SPR_PTR_LO >> 8) & 0xFF])           # LD (SPR_PTR_LO), A
        emit([0x3E, (spr_data_addr >> 8) & 0xFF])                            # LD A, hi
        emit([0x32, SPR_PTR_HI & 0xFF, (SPR_PTR_HI >> 8) & 0xFF])           # LD (SPR_PTR_HI), A

    # LD HL, row_addr_table
    emit([0x21, row_addr_table & 0xFF, (row_addr_table >> 8) & 0xFF])        # LD HL, row_addr_table
    # LD B, row_count
    emit([0x06, row_count])                                                   # LD B, row_count

    row_loop = len(code)
    asm.append(f"  .row_loop_{label}:  ; 0x{abs_addr(row_loop):04X}")

    # Read base row address from table → DE
    emit([0x5E])   # LD E, (HL)
    emit([0x23])   # INC HL
    emit([0x56])   # LD D, (HL)
    emit([0x23])   # INC HL
    # PUSH HL (save row-addr table pointer)
    emit([0xE5])   # PUSH HL

    # DE = base_addr + x_col
    # Load x_col from dedicated RAM cell, add to E with carry into D
    emit([0x3A, x_col_addr & 0xFF, (x_col_addr >> 8) & 0xFF])  # LD A, (x_col)
    emit([0x7B])                                                  # LD A, E
    emit([0x3A, x_col_addr & 0xFF, (x_col_addr >> 8) & 0xFF])  # LD A, (x_col)  [reload — see note]
    # Note: we need to ADD x_col to E.
    # Z80 can't ADD A, (nn) directly. We use: LD A, E / ADD A, x_col_reg.
    # But x_col is in memory. Load it to C temporarily (C is free here,
    # before we load bytes_per_row into it).
    # Undo the last two emits and do it properly:
    # Pop the last 5 bytes we just emitted (the two LD A, (x_col) + LD A, E)
    pass

# The inline address arithmetic keeps hitting the same register-pressure wall.
# Let's commit to the correct minimal approach and write it cleanly from scratch
# without intermediate corrections:
#
# KEY INSIGHT: We only need x_col in A for ONE ADD instruction per row.
# Use this sequence:
#   LD A, (x_col_addr)   ; A = x_col
#   ADD A, E             ; A = E + x_col  (low byte of addr + offset)
#   LD E, A              ; E = new low byte
#   JR NC, skip          ; if no carry, D is unchanged
#   INC D                ; else increment high byte
#   skip:
# This is 6 bytes, correct, and uses only A. No extra registers needed.

code.clear()
asm.clear()

def emit_blit_block(row_addr_table, spr_data_addr,
                    row_count, bytes_per_row, x_col_addr,
                    label, is_erase=False):
    """
    Emit an inline masked blit or solid erase block.

    Blit register contract:
      HL  = row-address table pointer (2 bytes/row: base addr at x=0)
      DE  = VRAM destination           (base_row_addr + x_col)
      B   = outer row counter          (DJNZ)
      C   = inner byte counter         (reloaded each row)
      RAM[SPR_PTR_LO/HI] = sprite data pointer (advances through data)
    """
    global asm
    asm.append(f"\n  ; --- {label} ---")

    if not is_erase:
        # Initialise sprite-data pointer in RAM
        lo = spr_data_addr & 0xFF
        hi = (spr_data_addr >> 8) & 0xFF
        emit([0x3E, lo])                                              # LD A, lo
        emit([0x32, SPR_PTR_LO & 0xFF, (SPR_PTR_LO >> 8) & 0xFF])   # LD (SPR_PTR_LO), A
        emit([0x3E, hi])                                              # LD A, hi
        emit([0x32, SPR_PTR_HI & 0xFF, (SPR_PTR_HI >> 8) & 0xFF])   # LD (SPR_PTR_HI), A

    emit([0x21, row_addr_table & 0xFF, (row_addr_table >> 8) & 0xFF])  # LD HL, row_addr_table
    emit([0x06, row_count])                                             # LD B, row_count

    row_loop = len(code)
    asm.append(f"  .row_loop_{label}:  ; 0x{abs_addr(row_loop):04X}")

    # Read base row address into DE
    emit([0x5E]); emit([0x23])   # LD E,(HL) / INC HL
    emit([0x56]); emit([0x23])   # LD D,(HL) / INC HL
    emit([0xE5])                 # PUSH HL  (save row-addr table ptr)

    # Add x_col to DE:  A = (x_col); A += E; E = A; if carry then INC D
    emit([0x3A, x_col_addr & 0xFF, (x_col_addr >> 8) & 0xFF])  # LD A, (x_col)
    emit([0x83])                                                  # ADD A, E
    emit([0x5F])                                                  # LD E, A
    emit([0x30, 0x01])                                            # JR NC, +1
    emit([0x14])                                                  # INC D

    # Load byte-counter into C
    emit([0x0E, bytes_per_row])   # LD C, bytes_per_row

    if is_erase:
        # --- ERASE INNER LOOP ---
        # Just write BG_BYTE to each VRAM byte in the row.
        emit([0x3E, BG_BYTE])   # LD A, BG_BYTE  (outside inner loop — A is constant)
        byte_loop = len(code)
        asm.append(f"    .byte_loop_{label}:  ; 0x{abs_addr(byte_loop):04X}")
        emit([0x12])             # LD (DE), A
        emit([0x13])             # INC DE
        emit([0x0D])             # DEC C
        emit(jr_back(byte_loop)) # JR NZ, byte_loop
    else:
        # --- BLIT INNER LOOP ---
        # Load sprite data pointer from RAM into HL (we saved row-addr ptr on stack)
        emit([0x2A, SPR_PTR_LO & 0xFF, (SPR_PTR_LO >> 8) & 0xFF])  # LD HL, (SPR_PTR)
        # LD HL, (nn) is a Z80 opcode: 0x2A lo hi — loads 16-bit from memory into HL.
        # This is NOT yet in our emulator. Use the two-byte alternative:
        #   LD L, (nn_lo)   [0x3A then store to L via A]
        #   LD H, (nn_hi)   [0x3A then store to H via A]
        # But LD L, A / LD H, A are register-register moves (0x6F, 0x67).
        # So:
        #   LD A, (SPR_PTR_LO) → LD L, A
        #   LD A, (SPR_PTR_HI) → LD H, A
        # Pop the LD HL,(nn) we just emitted and replace:
        pass

    return row_loop  # will be patched below

# Once again the LD HL,(nn) opcode (0x2A) is blocking us.
# Let's just ADD IT to the emulator — it's a real Z80 opcode we'll need anyway.
# But we want to avoid modifying main.cpp mid-session.
#
# FINAL clean solution without LD HL,(nn):
# Store the sprite data pointer as TWO separate bytes in RAM.
# Reconstruct into HL using:
#   LD A, (SPR_PTR_LO) → LD L, A   (2+1 = 3 bytes)
#   LD A, (SPR_PTR_HI) → LD H, A   (2+1 = 3 bytes)
# Then after the inner loop save back:
#   LD A, L → LD (SPR_PTR_LO), A
#   LD A, H → LD (SPR_PTR_HI), A
# This costs 12 bytes per row but is architecturally clean.
# We already have all the opcodes needed: 0x3A, 0x6F, 0x67, 0x7D, 0x7C, 0x32.

code.clear()
asm.clear()

def emit_blit_block(row_addr_table, spr_data_addr,
                    row_count, bytes_per_row, x_col_addr,
                    label, is_erase=False):
    """
    Emit an inline masked blit or solid erase block.

    Blit register contract per row:
      Stack        : saved row-addr-table HL (PUSH before inner loop, POP after)
      HL           : sprite data pointer during inner loop
      DE           : VRAM destination
      B            : outer row counter (DJNZ at bottom of outer loop)
      C            : inner byte counter
      A            : scratch
      RAM[SPR_PTR] : sprite data pointer persists across rows (lo/hi bytes)
      outer HL     : row-address table pointer (saved on stack during inner)

    After inner loop: save updated HL (sprite ptr) back to RAM, POP outer HL.
    """
    global asm
    asm.append(f"\n  ; === {label} {'(erase)' if is_erase else '(blit)'} ===")

    if not is_erase:
        lo = spr_data_addr & 0xFF
        hi = (spr_data_addr >> 8) & 0xFF
        emit([0x3E, lo],  f"LD A, 0x{lo:02X}  ; init sprite ptr lo")
        emit([0x32, SPR_PTR_LO & 0xFF, (SPR_PTR_LO >> 8) & 0xFF],
             "LD (SPR_PTR_LO), A")
        emit([0x3E, hi],  f"LD A, 0x{hi:02X}  ; init sprite ptr hi")
        emit([0x32, SPR_PTR_HI & 0xFF, (SPR_PTR_HI >> 8) & 0xFF],
             "LD (SPR_PTR_HI), A")

    emit([0x21, row_addr_table & 0xFF, (row_addr_table >> 8) & 0xFF],
         f"LD HL, 0x{row_addr_table:04X}  ; row-addr table")
    emit([0x06, row_count], f"LD B, {row_count}  ; row counter")

    row_loop = len(code)
    asm.append(f"  .row_{label}:")   # label for ASM listing

    # 1. Read base row address → DE
    emit([0x5E],       "LD E, (HL)")
    emit([0x23],       "INC HL")
    emit([0x56],       "LD D, (HL)")
    emit([0x23],       "INC HL")
    emit([0xE5],       "PUSH HL  ; save row-addr table ptr")

    # 2. DE += x_col
    emit([0x3A, x_col_addr & 0xFF, (x_col_addr >> 8) & 0xFF], "LD A, (x_col)")
    emit([0x83],       "ADD A, E")
    emit([0x5F],       "LD E, A")
    emit([0x30, 0x01], "JR NC, +1  ; skip INC D if no carry")
    emit([0x14],       "INC D")

    # 3. Load inner byte counter
    emit([0x0E, bytes_per_row], f"LD C, {bytes_per_row}")

    if is_erase:
        # 4e. Erase inner loop: write BG_BYTE to each byte of this VRAM row
        emit([0x3E, BG_BYTE], f"LD A, 0x{BG_BYTE:02X}  ; background byte")
        byte_loop = len(code)
        asm.append(f"    .ebyte_{label}:")
        emit([0x12],             "LD (DE), A")
        emit([0x13],             "INC DE")
        emit([0x0D],             "DEC C")
        emit(jr_back(byte_loop), "JR NZ, .ebyte")
    else:
        # 4b. Reconstruct sprite data pointer into HL from RAM
        emit([0x3A, SPR_PTR_LO & 0xFF, (SPR_PTR_LO >> 8) & 0xFF], "LD A, (SPR_PTR_LO)")
        emit([0x6F],             "LD L, A")
        emit([0x3A, SPR_PTR_HI & 0xFF, (SPR_PTR_HI >> 8) & 0xFF], "LD A, (SPR_PTR_HI)")
        emit([0x67],             "LD H, A")

        # Save outer row counter B to RAM before inner loop clobbers it.
        # The inner loop uses LD B,(HL) for mask bytes, destroying B.
        # We use a dedicated RAM cell: ROW_CTR_SAVE = 0xB008
        ROW_CTR_SAVE = 0xB008
        emit([0x78],             "LD A, B  ; save row counter")
        emit([0x32, ROW_CTR_SAVE & 0xFF, (ROW_CTR_SAVE >> 8) & 0xFF],
             "LD (ROW_CTR_SAVE), A")

        # 5b. Blit inner loop
        byte_loop = len(code)
        asm.append(f"    .bbyte_{label}:")
        # B_inner = mask, A = sprite byte
        emit([0x46],             "LD B, (HL)   ; mask byte")
        emit([0x23],             "INC HL")
        emit([0x7E],             "LD A, (HL)   ; sprite byte")
        emit([0x23],             "INC HL")
        emit([0xE5],             "PUSH HL      ; save sprite data ptr")
        emit([0x6F],             "LD L, A      ; move sprite byte to L")
        emit([0x1A],             "LD A, (DE)   ; read VRAM background")
        emit([0xA0],             "AND B        ; clear opaque pixels")
        emit([0xB5],             "OR  L        ; write sprite pixels")
        emit([0x12],             "LD (DE), A   ; write back to VRAM")
        emit([0xE1],             "POP HL       ; restore sprite data ptr")
        emit([0x13],             "INC DE")
        emit([0x0D],             "DEC C")
        emit(jr_back(byte_loop), "JR NZ, .bbyte")

        # 6b. Save updated sprite data pointer back to RAM
        emit([0x7D],             "LD A, L")
        emit([0x32, SPR_PTR_LO & 0xFF, (SPR_PTR_LO >> 8) & 0xFF], "LD (SPR_PTR_LO), A")
        emit([0x7C],             "LD A, H")
        emit([0x32, SPR_PTR_HI & 0xFF, (SPR_PTR_HI >> 8) & 0xFF], "LD (SPR_PTR_HI), A")

        # Restore outer row counter B from RAM
        emit([0x3A, ROW_CTR_SAVE & 0xFF, (ROW_CTR_SAVE >> 8) & 0xFF],
             "LD A, (ROW_CTR_SAVE)  ; restore row counter")
        emit([0x47],             "LD B, A")

    # 7. Restore row-addr table pointer from stack, decrement row counter
    emit([0xE1],               "POP HL  ; restore row-addr table ptr")
    emit(djnz_back(row_loop),  "DJNZ .row")

# ---------------------------------------------------------------------------
# Main program assembly
# ---------------------------------------------------------------------------

# --- Phase 0: Initialise stack and RAM variables ---
emit([0x31, 0x00, 0xF0],   "LD SP, 0xF000  ; initialise stack")

# alig_x = ALIG_X_START
emit([0x3E, ALIG_X_START])
emit([0x32, ALIG_X_ADDR & 0xFF, (ALIG_X_ADDR >> 8) & 0xFF])
# hero_x = HERO_X_START
emit([0x3E, HERO_X_START])
emit([0x32, HERO_X_ADDR & 0xFF, (HERO_X_ADDR >> 8) & 0xFF])
# frame_ctr = 0
emit([0xAF])   # XOR A  (A = 0)
emit([0x32, FRAME_CTR_ADDR & 0xFF, (FRAME_CTR_ADDR >> 8) & 0xFF])

# --- Phase 1: Fill entire VRAM with background colour (once, before frame loop) ---
asm.append("\n  ; === Initial VRAM fill ===")
emit([0x21, FILL_TABLE & 0xFF, (FILL_TABLE >> 8) & 0xFF])   # LD HL, FILL_TABLE
emit([0x11, 0x00, 0xC0])                                     # LD DE, 0xC000
emit([0x01, 0x00, 0x40])                                     # LD BC, 0x4000
emit([0xED, 0xB0])                                           # LDIR

# --- Draw ground line once (it never moves) ---
asm.append("\n  ; === Draw ground (once) ===")
emit([0x21, GROUND_TABLE & 0xFF, (GROUND_TABLE >> 8) & 0xFF])
emit([0x06, GROUND_H])  # LD B, row_count
ground_row_loop = len(code)
emit([0x5E]); emit([0x23])   # LD E,(HL) / INC HL
emit([0x56]); emit([0x23])   # LD D,(HL) / INC HL
emit([0x0E, GROUND_W])       # LD C, 80
ground_byte_loop = len(code)
emit([0x7E]); emit([0x23])   # LD A,(HL) / INC HL
emit([0x12])                  # LD (DE), A
emit([0x13])                  # INC DE
emit([0x0D])                  # DEC C
emit(jr_back(ground_byte_loop))
emit(djnz_back(ground_row_loop))

# --- Phase 2: Outer frame loop (B = NUM_FRAMES) ---
asm.append(f"\n  ; === Frame loop: {NUM_FRAMES} frames ===")
emit([0x06, NUM_FRAMES])   # LD B, NUM_FRAMES
frame_loop = len(code)
asm.append(f"  .frame_loop:  ; 0x{abs_addr(frame_loop):04X}")

# Save frame loop counter B — DJNZ uses B, but our inner blits also use B.
# We need to preserve B across the per-frame work.
# Strategy: save B (frame counter) to RAM at the start of each frame,
# restore it just before DJNZ.
FRAME_COUNTER_B = 0xB007   # dedicated RAM cell for the frame-loop B register
emit([0x78])                                                              # LD A, B
emit([0x32, FRAME_COUNTER_B & 0xFF, (FRAME_COUNTER_B >> 8) & 0xFF])     # LD (FRAME_COUNTER_B), A

# --- 2a. Erase alligator at current position ---
emit([0x3A, ALIG_X_ADDR & 0xFF, (ALIG_X_ADDR >> 8) & 0xFF])   # LD A, (alig_x)
emit([0x32, X_COL_SCRATCH & 0xFF, (X_COL_SCRATCH >> 8) & 0xFF])  # LD (X_COL_SCRATCH), A
emit_blit_block(ALIG_ROW_ADDRS, None, ALIG_H, ALIG_BYTES,
                X_COL_SCRATCH, "erase_alig", is_erase=True)

# --- 2b. Erase hero at current position ---
emit([0x3A, HERO_X_ADDR & 0xFF, (HERO_X_ADDR >> 8) & 0xFF])
emit([0x32, X_COL_SCRATCH & 0xFF, (X_COL_SCRATCH >> 8) & 0xFF])
emit_blit_block(HERO_ROW_ADDRS, None, HERO_H, HERO_BYTES,
                X_COL_SCRATCH, "erase_hero", is_erase=True)

# --- 2c. Update alligator x: alig_x -= 1 (clamp to ALIG_X_MIN) ---
asm.append("\n  ; --- update alig_x ---")
emit([0x3A, ALIG_X_ADDR & 0xFF, (ALIG_X_ADDR >> 8) & 0xFF])   # LD A, (alig_x)
emit([0xFE, ALIG_X_MIN])                                         # CP ALIG_X_MIN
skip_alig_dec = len(code)
emit([0x28, 0x00])                                               # JR Z, skip (patched below)
emit([0x3D])                                                     # DEC A
emit([0x32, ALIG_X_ADDR & 0xFF, (ALIG_X_ADDR >> 8) & 0xFF])   # LD (alig_x), A
# Patch the JR Z displacement
skip_target = len(code)
code[skip_alig_dec + 1] = (skip_target - (skip_alig_dec + 2)) & 0xFF

# --- 2d. Update hero x: hero_x += 1 (clamp to HERO_X_MAX) ---
asm.append("\n  ; --- update hero_x ---")
emit([0x3A, HERO_X_ADDR & 0xFF, (HERO_X_ADDR >> 8) & 0xFF])   # LD A, (hero_x)
emit([0xFE, HERO_X_MAX])                                         # CP HERO_X_MAX
skip_hero_inc = len(code)
emit([0x28, 0x00])                                               # JR Z, skip (patched)
emit([0x3C])                                                     # INC A
emit([0x32, HERO_X_ADDR & 0xFF, (HERO_X_ADDR >> 8) & 0xFF])   # LD (hero_x), A
skip_target = len(code)
code[skip_hero_inc + 1] = (skip_target - (skip_hero_inc + 2)) & 0xFF

# --- 2e. Choose animation frame (bit 1 of frame_ctr selects A vs B) ---
# Walk cycle: switch every 4 display frames for natural gait tempo.
# Bit 1 of frame_ctr: 0→frame A, 1→frame B.
asm.append("\n  ; --- select animation frame ---")
emit([0x3A, FRAME_CTR_ADDR & 0xFF, (FRAME_CTR_ADDR >> 8) & 0xFF])  # LD A, (frame_ctr)
emit([0xE6, 0x04])                                                    # AND 0x04  ; test bit 2
# If zero → use frame A data; if non-zero → use frame B data.
# We emit TWO conditional blit calls:
#   - One blit block for frame A (skipped if bit set)
#   - One blit block for frame B (skipped if bit clear)
# Use JP NZ to skip frame A block, JP Z to skip frame B block.
# We need forward jumps — use JP with patched addresses.

# Frame A alligator blit
asm.append("\n  ; --- alig frame A ---")
emit([0x3A, ALIG_X_ADDR & 0xFF, (ALIG_X_ADDR >> 8) & 0xFF])
emit([0x32, X_COL_SCRATCH & 0xFF, (X_COL_SCRATCH >> 8) & 0xFF])
# Reload frame_ctr test result (AND already set Z flag)
emit([0x3A, FRAME_CTR_ADDR & 0xFF, (FRAME_CTR_ADDR >> 8) & 0xFF])
emit([0xE6, 0x04])
jp_skip_alig_A = len(code)
emit([0xC2, 0x00, 0x00])   # JP NZ, <skip_alig_A>  (patched)
emit_blit_block(ALIG_ROW_ADDRS, ALIG_DATA_A, ALIG_H, ALIG_BYTES,
                X_COL_SCRATCH, "blit_alig_A")
jp_skip_alig_A_target = len(code)
code[jp_skip_alig_A + 1] = abs_addr(jp_skip_alig_A_target) & 0xFF
code[jp_skip_alig_A + 2] = (abs_addr(jp_skip_alig_A_target) >> 8) & 0xFF

# Frame B alligator blit
asm.append("\n  ; --- alig frame B ---")
emit([0x3A, ALIG_X_ADDR & 0xFF, (ALIG_X_ADDR >> 8) & 0xFF])
emit([0x32, X_COL_SCRATCH & 0xFF, (X_COL_SCRATCH >> 8) & 0xFF])
emit([0x3A, FRAME_CTR_ADDR & 0xFF, (FRAME_CTR_ADDR >> 8) & 0xFF])
emit([0xE6, 0x04])
jp_skip_alig_B = len(code)
emit([0xCA, 0x00, 0x00])   # JP Z, <skip_alig_B>  (patched)
emit_blit_block(ALIG_ROW_ADDRS, ALIG_DATA_B, ALIG_H, ALIG_BYTES,
                X_COL_SCRATCH, "blit_alig_B")
jp_skip_alig_B_target = len(code)
code[jp_skip_alig_B + 1] = abs_addr(jp_skip_alig_B_target) & 0xFF
code[jp_skip_alig_B + 2] = (abs_addr(jp_skip_alig_B_target) >> 8) & 0xFF

# Frame A hero blit
asm.append("\n  ; --- hero frame A ---")
emit([0x3A, HERO_X_ADDR & 0xFF, (HERO_X_ADDR >> 8) & 0xFF])
emit([0x32, X_COL_SCRATCH & 0xFF, (X_COL_SCRATCH >> 8) & 0xFF])
emit([0x3A, FRAME_CTR_ADDR & 0xFF, (FRAME_CTR_ADDR >> 8) & 0xFF])
emit([0xE6, 0x04])
jp_skip_hero_A = len(code)
emit([0xC2, 0x00, 0x00])   # JP NZ, <skip_hero_A>
emit_blit_block(HERO_ROW_ADDRS, HERO_DATA_A, HERO_H, HERO_BYTES,
                X_COL_SCRATCH, "blit_hero_A")
jp_skip_hero_A_target = len(code)
code[jp_skip_hero_A + 1] = abs_addr(jp_skip_hero_A_target) & 0xFF
code[jp_skip_hero_A + 2] = (abs_addr(jp_skip_hero_A_target) >> 8) & 0xFF

# Frame B hero blit
asm.append("\n  ; --- hero frame B ---")
emit([0x3A, HERO_X_ADDR & 0xFF, (HERO_X_ADDR >> 8) & 0xFF])
emit([0x32, X_COL_SCRATCH & 0xFF, (X_COL_SCRATCH >> 8) & 0xFF])
emit([0x3A, FRAME_CTR_ADDR & 0xFF, (FRAME_CTR_ADDR >> 8) & 0xFF])
emit([0xE6, 0x04])
jp_skip_hero_B = len(code)
emit([0xCA, 0x00, 0x00])   # JP Z, <skip_hero_B>
emit_blit_block(HERO_ROW_ADDRS, HERO_DATA_B, HERO_H, HERO_BYTES,
                X_COL_SCRATCH, "blit_hero_B")
jp_skip_hero_B_target = len(code)
code[jp_skip_hero_B + 1] = abs_addr(jp_skip_hero_B_target) & 0xFF
code[jp_skip_hero_B + 2] = (abs_addr(jp_skip_hero_B_target) >> 8) & 0xFF

# --- 2f. Increment frame counter, wrap at 8 (one full walk cycle = 8 frames) ---
asm.append("\n  ; --- increment frame_ctr (mod 8) ---")
emit([0x3A, FRAME_CTR_ADDR & 0xFF, (FRAME_CTR_ADDR >> 8) & 0xFF])  # LD A, (frame_ctr)
emit([0x3C])                                                          # INC A
emit([0xE6, 0x07])                                                    # AND 0x07  ; mod 8
emit([0x32, FRAME_CTR_ADDR & 0xFF, (FRAME_CTR_ADDR >> 8) & 0xFF])  # LD (frame_ctr), A

# --- 2g. OUT (0), A — signal emulator to dump VRAM frame ---
asm.append("\n  ; --- frame sync: dump VRAM ---")
emit([0xD3, 0x00])   # OUT (0), A

# --- 2h. Restore frame-loop B counter and DJNZ ---
emit([0x3A, FRAME_COUNTER_B & 0xFF, (FRAME_COUNTER_B >> 8) & 0xFF])  # LD A, (FRAME_COUNTER_B)
emit([0x47])                                                             # LD B, A

# Frame loop is >128 bytes — DJNZ can't reach it. Use DEC B + JP NZ instead.
emit([0x05],  "DEC B")
jp_frame_loop = len(code)
emit([0xC2, 0x00, 0x00],  "JP NZ, .frame_loop  (patched)")
code[jp_frame_loop + 1] = abs_addr(frame_loop) & 0xFF
code[jp_frame_loop + 2] = (abs_addr(frame_loop) >> 8) & 0xFF


# --- End ---
emit([0x76], "HALT")

# ---------------------------------------------------------------------------
# Assemble into program binary
# ---------------------------------------------------------------------------
print(f"\nCode size: {len(code)} bytes (starts at 0x{CODE_START:04X}, ends at 0x{CODE_START+len(code):04X})")
assert CODE_START + len(code) < BINARY_SIZE, \
    f"Code overflow: 0x{CODE_START+len(code):04X} >= 0x{BINARY_SIZE:04X}"

for i, b in enumerate(code):
    program[CODE_START + i] = b

# Cycle estimate
# Per frame: ~2 blits (masked) + 2 erases + overhead
# Masked blit: ALIG_H * (ALIG_BYTES * ~15 instr) ≈ 16*12*15 = 2880
# Erase:       ALIG_H * (ALIG_BYTES * ~4  instr)  ≈ 16*12*4  = 768
# Hero blit:   20*8*15 = 2400; erase: 20*8*4 = 640
# Overhead + frame logic: ~200
# Per frame total: ~(2880+768+2400+640)*2 + 200 ≈ 13576 (only one set runs per frame)
# More accurately: 2880+768+2400+640 + overhead ≈ 6900 per frame
# 48 frames: ~330000 cycles — well within MAX_CYCLES=1000000

# ---------------------------------------------------------------------------
# Write binary
# ---------------------------------------------------------------------------
bin_path = "programs/lesson10.bin"
with open(bin_path, "wb") as f:
    f.write(program)

# ---------------------------------------------------------------------------
# Write ASM listing
# ---------------------------------------------------------------------------
asm_header = [
    "; lesson10.asm  --  AUTO-GENERATED by gen_lesson10.py  --  DO NOT EDIT",
    "; WPS-Z80 Lesson 10: Movement — sprite erase/redraw loop, 48 VRAM frames",
    ";",
    f"; Alligator: 24x16 px, facing left, starts byte col {ALIG_X_START}, moves left",
    f"; Hero:      16x20 px, facing right, starts byte col {HERO_X_START}, moves right",
    f"; Frames:    {NUM_FRAMES}",
    ";",
    "; New opcodes: DJNZ (0x10), OUT (n),A (0xD3)",
    ";",
    "; Blit strategy:",
    ";   Row-address tables hold base VRAM addresses (x=0) per sprite row.",
    ";   X byte-column is stored in RAM and added to each base address at runtime.",
    ";   Sprite data tables hold (mask, spr_byte) pairs — no addresses embedded.",
    ";   PUSH HL / POP HL preserves the row-addr table pointer across inner loops.",
    ";   Sprite data pointer lives in RAM (SPR_PTR_LO/HI) across rows.",
    ";",
    f"; RAM: alig_x=0x{ALIG_X_ADDR:04X}  hero_x=0x{HERO_X_ADDR:04X}  "
    f"frame_ctr=0x{FRAME_CTR_ADDR:04X}  spr_ptr=0x{SPR_PTR_LO:04X}/0x{SPR_PTR_HI:04X}",
]

asm_path = "programs/lesson10.asm"
with open(asm_path, "w") as f:
    f.write("\n".join(asm_header) + "\n")
    f.write("\n".join(asm) + "\n")

print(f"Generated: {bin_path}  ({len(program)} bytes)")
print(f"Generated: {asm_path}")
print()
print("To build and run (PowerShell):")
print("  g++ main.cpp -o emulator.exe -std=c++17")
print("  g++ monitor.cpp -o monitor.exe -I SDL3/include -L SDL3/lib -lSDL3 -std=c++17")
print("  python programs\\gen_lesson10.py")
print("  Remove-Item -Recurse -Force programs\\lesson10_vram -ErrorAction SilentlyContinue")
print("  Start-Process -FilePath '.\\monitor.exe' -ArgumentList 'programs\\lesson10_vram --mode 0'")
print("  .\\emulator.exe programs\\lesson10.bin --notrace")
print()
print(f"Expected: {NUM_FRAMES} VRAM frames in programs/lesson10_vram/")
print("  frame_0001.vram — alligator at col 59, hero at col 9")
print(f"  frame_0026.vram — sprites meet near centre")
print(f"  frame_0048.vram — alligator clamped left, hero near col 56")