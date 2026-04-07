# gen_lesson9.py  (v4 — verified orientations)
# WPS-Z80 Assembler Script - Lesson 9: Sprite Blitting with Transparency
# Generates lesson9.bin and lesson9.asm
#
# Scene:
#   - Dark blue background (pen 1)
#   - Yellow-brown ground line at y=148 (pen 12)
#   - Alligator facing LEFT  at byte col 34, row 132
#   - Hero    facing RIGHT   at byte col  8, row 128
#
# Sprite encoding: CPC Mode 0, 2 pixels per byte.
# Transparency:    pen 0 = transparent (AND mask + OR sprite blit).
# Interleaved table per row: [dest_lo, dest_hi, mask0, spr0, ..., maskW, sprW]
#
# Verified pen encoding (solid_mode0_byte):
#   pen  0 = 0x00   pen  1 = 0xC0   pen  2 = 0x0C   pen  3 = 0xCC
#   pen  4 = 0x30   pen  5 = 0xF0   pen  6 = 0x3C   pen  7 = 0xFC
#   pen  8 = 0x03   pen  9 = 0xC3   pen 10 = 0x0F   pen 11 = 0xCF
#   pen 12 = 0x33   pen 13 = 0xF3   pen 14 = 0x3F   pen 15 = 0xFF
#
# Verified Mode 0 decode (monitor.cpp):
#   pens[0] = (b>>7&1) | ((b>>3&1)<<1) | ((b>>5&1)<<2) | ((b>>1&1)<<3)
#   pens[1] = (b>>6&1) | ((b>>2&1)<<1) | ((b>>4&1)<<2) | ((b>>0&1)<<3)

# ---------------------------------------------------------------------------
# Encoding helpers
# ---------------------------------------------------------------------------

def solid_mode0_byte(pen):
    """Encode a solid Mode 0 byte where both pixels have the same pen."""
    p = [(pen >> i) & 1 for i in range(4)]
    return (p[0]<<7)|(p[0]<<6)|(p[2]<<5)|(p[2]<<4)|(p[1]<<3)|(p[1]<<2)|(p[3]<<1)|p[3]

def two_mode0_pixels(pl, pr):
    """Encode two different Mode 0 pixels into one byte."""
    l = [(pl >> i) & 1 for i in range(4)]
    r = [(pr >> i) & 1 for i in range(4)]
    return (l[0]<<7)|(r[0]<<6)|(l[2]<<5)|(r[2]<<4)|(l[1]<<3)|(r[1]<<2)|(l[3]<<1)|r[3]

def mode0_mask(pl, pr, tp=0):
    """
    AND mask byte for a pixel pair.
    Bits are 0 where pixel is opaque (will be cleared before OR).
    Bits are 1 where pixel is transparent (background preserved).
    """
    lt = (pl == tp)
    rt = (pr == tp)
    mask = 0
    if lt: mask |= (1<<7)|(1<<5)|(1<<3)|(1<<1)
    if rt: mask |= (1<<6)|(1<<4)|(1<<2)|(1<<0)
    return mask

def crtc_abs(y, x):
    """Absolute VRAM address for pixel row y, byte column x."""
    return 0xC000 + (y % 8) * 0x0800 + (y // 8) * 0x0050 + x

def mirror_sprite(grid):
    """Horizontally mirror a sprite pixel grid."""
    return [row[::-1] for row in grid]

def encode_row_with_mask(row, tp=0):
    """Encode one pixel row into list of (mask, sprite_byte) pairs."""
    assert len(row) % 2 == 0, f"Row width {len(row)} is not even"
    pairs = []
    for x in range(0, len(row), 2):
        pl, pr = row[x], row[x+1]
        pairs.append((mode0_mask(pl, pr, tp), two_mode0_pixels(pl, pr)))
    return pairs

def make_interleaved(pixel_grid, vram_addrs, tp=0):
    """
    Build interleaved blit table.
    Per row: [dest_lo, dest_hi, mask0, spr0, mask1, spr1, ...]
    """
    data = bytearray()
    for row, addr in zip(pixel_grid, vram_addrs):
        pairs = encode_row_with_mask(row, tp)
        data += bytes([addr & 0xFF, (addr >> 8) & 0xFF])
        for mask, spr in pairs:
            data += bytes([mask, spr])
    return data

# ---------------------------------------------------------------------------
# Pen constants (CPC Mode 0 default firmware palette)
# ---------------------------------------------------------------------------
BK = 0   # black / transparent
G1 = 9   # green        (alligator body)
G2 = 15  # bright green (alligator ridge)
WH = 13  # white/grey   (alligator belly, teeth)
YL = 12  # yellow       (alligator eye)
RD = 6   # bright red   (mouth cavity)
PK = 7   # bright magenta (tongue)

HAIR = 3   # red-brown  (hero hair)
SKIN = 8   # bright mauve (hero skin — warmest CPC approximation)
SHIRT = 6  # bright red (hero shirt)
TROU = 2   # bright blue (hero trousers)
SHOE = 13  # white/grey  (hero shoes)

# Single-letter shorthands for hero pixel grids
H = HAIR
K = SKIN
O = SHIRT
N = TROU
S = SHOE
_ = BK   # transparent

# ---------------------------------------------------------------------------
# ALLIGATOR pixel grids — drawn facing RIGHT, then mirrored to face LEFT
#
# In the "facing right" drawing:
#   column 0  = tail tip (leftmost)
#   column 23 = snout tip (rightmost)
#
# After mirror_sprite:
#   column 0  = snout tip (leftmost)  <- correct for left-facing
#   column 23 = tail tip (rightmost)
#
# Sprite size: 24 pixels wide x 16 rows tall = 12 bytes x 16 rows
# ---------------------------------------------------------------------------

# FRAME 1: walk A, mouth closed
_ALIG_CLOSED_A = [
# col:  0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17  18  19  20  21  22  23
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

# FRAME 2: walk A, mouth OPEN
_ALIG_OPEN_A = [
      [ _,  _,  _,  _,  G2, _,  G2, _,  G2, _,  G2, _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _],
      [ _,  _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, _,  _,  _,  _,  _,  _,  _,  _],
      [ _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, _,  _],
      [ _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, YL, G1, G1, _],
      [ _,  G1, G1, G1, WH, WH, WH, WH, WH, WH, WH, WH, G1, G1, G1, G1, G1, G1, G1, G1, _,  G1, G1, G1],
      [ _,  G1, G1, G1, WH, WH, WH, WH, WH, WH, WH, WH, G1, G1, G1, G1, G1, G1, _,  WH, _,  WH, _,  WH],
      [ _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, RD, RD, RD, RD, RD, RD, RD, RD, RD, RD, RD, RD],
      [ _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, RD, RD, PK, PK, PK, PK, RD, RD, RD, RD, RD, RD],
      [ _,  G1, G1, G1, WH, WH, WH, WH, WH, WH, WH, WH, G1, G1, G1, G1, G1, G1, _,  WH, _,  WH, _,  WH],
      [ _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1],
      [ _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, _],
      [ _,  _,  G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, G1, _,  _],
      [ _,  _,  _,  G1, _,  _,  _,  G1, _,  _,  _,  _,  _,  _,  _,  _,  G1, _,  _,  _,  G1, _,  _,  _],
      [ _,  _,  _,  G1, _,  _,  _,  G1, _,  _,  _,  _,  _,  _,  _,  _,  G1, _,  _,  _,  G1, _,  _,  _],
      [ _,  _,  G1, G1, G1, _,  _,  G1, G1, G1, _,  _,  _,  _,  _,  G1, G1, G1, _,  _,  G1, G1, G1, _],
      [ _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _],
]

# FRAME 3: walk B, mouth closed (legs in opposite phase)
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

# FRAME 4: walk B, mouth open
_ALIG_OPEN_B = [
      *_ALIG_OPEN_A[:12],
      *_ALIG_CLOSED_B[12:],
]

# Mirror all alligator frames to face LEFT (snout on left side)
ALIG_CLOSED_A = mirror_sprite(_ALIG_CLOSED_A)
ALIG_OPEN_A   = mirror_sprite(_ALIG_OPEN_A)
ALIG_CLOSED_B = mirror_sprite(_ALIG_CLOSED_B)
ALIG_OPEN_B   = mirror_sprite(_ALIG_OPEN_B)

# ---------------------------------------------------------------------------
# HERO pixel grids — drawn natively facing RIGHT
# Sprite size: 16 pixels wide x 20 rows tall = 8 bytes x 20 rows
#
# column 0  = leftmost (back of hero)
# column 15 = rightmost (front of hero, direction of travel)
# Eyes on right side of face = facing right
# ---------------------------------------------------------------------------
HERO_RUN_A = [
# col:  0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
      [ _,  _,  _,  H,  H,  H,  H,  H,  H,  H,  H,  H,  H,  _,  _,  _],  # hair
      [ _,  _,  H,  H,  H,  H,  H,  H,  H,  H,  H,  H,  H,  H,  _,  _],  # hair
      [ _,  _,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  _,  _],  # face
      [ _,  _,  K,  K,  K,  K,  K,  _,  K,  K,  K,  K,  _,  K,  _,  _],  # eyes (right side)
      [ _,  _,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  _,  _],  # face
      [ _,  _,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  _,  _],  # face
      [ _,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  _],  # shoulders
      [ O,  O,  O,  O,  K,  K,  K,  K,  K,  K,  K,  K,  O,  O,  O,  O],  # collar
      [ O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O],  # body
      [ O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O],  # body
      [ _,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  _],  # body
      [ _,  _,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  _,  _],  # body bottom
      [ _,  _,  N,  N,  N,  N,  _,  _,  _,  _,  N,  N,  N,  N,  _,  _],  # legs A
      [ _,  _,  N,  N,  N,  N,  _,  _,  _,  _,  N,  N,  N,  N,  _,  _],
      [ _,  _,  N,  N,  N,  N,  _,  _,  _,  _,  N,  N,  N,  N,  _,  _],
      [ _,  _,  _,  N,  N,  N,  _,  _,  N,  N,  N,  N,  _,  _,  _,  _],
      [ _,  _,  _,  N,  N,  N,  _,  _,  N,  N,  N,  N,  _,  _,  _,  _],
      [ _,  _,  _,  N,  N,  N,  _,  _,  _,  N,  N,  N,  _,  _,  _,  _],
      [ _,  _,  S,  S,  S,  S,  _,  _,  _,  _,  S,  S,  S,  S,  _,  _],  # shoes
      [ _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _],  # blank
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
      [ _,  _,  _,  N,  N,  N,  _,  _,  N,  N,  N,  N,  _,  _,  _,  _],  # legs B
      [ _,  _,  _,  N,  N,  N,  _,  _,  N,  N,  N,  N,  _,  _,  _,  _],
      [ _,  _,  _,  N,  N,  N,  _,  _,  N,  N,  N,  N,  _,  _,  _,  _],
      [ _,  _,  _,  _,  N,  N,  N,  N,  N,  N,  _,  _,  _,  _,  _,  _],
      [ _,  _,  _,  _,  N,  N,  N,  N,  N,  N,  _,  _,  _,  _,  _,  _],
      [ _,  _,  _,  _,  N,  N,  N,  N,  N,  N,  _,  _,  _,  _,  _,  _],
      [ _,  _,  _,  _,  S,  S,  S,  S,  S,  S,  _,  _,  _,  _,  _,  _],  # shoes
      [ _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _],
]

HERO_JUMP = [
      [ _,  _,  _,  H,  H,  H,  H,  H,  H,  H,  H,  H,  H,  _,  _,  _],
      [ _,  _,  H,  H,  H,  H,  H,  H,  H,  H,  H,  H,  H,  H,  _,  _],
      [ _,  _,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  _,  _],
      [ _,  _,  K,  K,  K,  K,  K,  _,  K,  K,  K,  K,  _,  K,  _,  _],
      [ _,  _,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  _,  _],
      [ K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K,  K],  # arms out
      [ O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O],
      [ O,  O,  O,  O,  K,  K,  K,  K,  K,  K,  K,  K,  O,  O,  O,  O],
      [ O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O],
      [ _,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  _],
      [ _,  _,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  _,  _],
      [ _,  _,  _,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  _,  _,  _],
      [ _,  _,  _,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  _,  _,  _],  # legs tucked
      [ _,  _,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  _,  _],
      [ _,  N,  N,  N,  S,  S,  S,  S,  S,  S,  S,  S,  N,  N,  N,  _],  # shoes visible
      [ _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _],
      [ _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _],
      [ _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _],
      [ _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _],
      [ _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _],
]

# ---------------------------------------------------------------------------
# Validate sprite dimensions
# ---------------------------------------------------------------------------
for name, grid, expected_w, expected_h in [
    ("ALIG_CLOSED_A", ALIG_CLOSED_A, 24, 16),
    ("ALIG_OPEN_A",   ALIG_OPEN_A,   24, 16),
    ("ALIG_CLOSED_B", ALIG_CLOSED_B, 24, 16),
    ("ALIG_OPEN_B",   ALIG_OPEN_B,   24, 16),
    ("HERO_RUN_A",    HERO_RUN_A,    16, 20),
    ("HERO_RUN_B",    HERO_RUN_B,    16, 20),
    ("HERO_JUMP",     HERO_JUMP,     16, 20),
]:
    assert len(grid) == expected_h, \
        f"{name}: expected {expected_h} rows, got {len(grid)}"
    for i, row in enumerate(grid):
        assert len(row) == expected_w, \
            f"{name} row {i}: expected {expected_w} pixels, got {len(row)}"

print("All sprite dimensions validated.")

# ---------------------------------------------------------------------------
# Scene layout
# ---------------------------------------------------------------------------
BG_PEN     = 1      # dark blue background
BG_BYTE    = solid_mode0_byte(BG_PEN)
GROUND_PEN = 12     # yellow-brown ground
GROUND_BYTE = solid_mode0_byte(GROUND_PEN)

ALIG_W     = 24
ALIG_H     = 16
ALIG_BYTES = ALIG_W // 2   # 12 bytes per row

HERO_W     = 16
HERO_H     = 20
HERO_BYTES = HERO_W // 2   # 8 bytes per row

GROUND_Y   = 148
GROUND_H   = 4
GROUND_W   = 80   # full 160px width = 80 bytes

ALIG_Y     = GROUND_Y - ALIG_H    # = 132
ALIG_X     = 34                    # byte col → pixel 68

HERO_Y     = GROUND_Y - HERO_H    # = 128
HERO_X     = 8                     # byte col → pixel 16

# VRAM address lists
alig_addrs   = [crtc_abs(ALIG_Y + r,   ALIG_X) for r in range(ALIG_H)]
hero_addrs   = [crtc_abs(HERO_Y + r,   HERO_X) for r in range(HERO_H)]
ground_addrs = [crtc_abs(GROUND_Y + r, 0)      for r in range(GROUND_H)]

# Encode interleaved tables
alig_table = make_interleaved(ALIG_CLOSED_A, alig_addrs)
hero_table = make_interleaved(HERO_RUN_A,    hero_addrs)

# Ground table: [dest_lo, dest_hi, byte * 80] per row — no masking
ground_table = bytearray()
for addr in ground_addrs:
    ground_table += bytes([addr & 0xFF, (addr >> 8) & 0xFF])
    ground_table += bytes([GROUND_BYTE] * GROUND_W)

# ---------------------------------------------------------------------------
# Memory layout
#
# Bytes per interleaved row:
#   Alligator: 2 + 12*2 = 26 bytes/row * 16 rows = 416 bytes
#   Hero:      2 +  8*2 = 18 bytes/row * 20 rows = 360 bytes
#   Ground:    2 + 80   = 82 bytes/row *  4 rows = 328 bytes
# ---------------------------------------------------------------------------
FILL_TABLE    = 0x0100
ALIG_TABLE    = 0x4200
HERO_TABLE    = 0x4400
GROUND_TABLE  = 0x4600
CODE_START    = 0x4800

print(f"Alligator table: {len(alig_table)} bytes  (max {HERO_TABLE-ALIG_TABLE})")
print(f"Hero table:      {len(hero_table)} bytes  (max {GROUND_TABLE-HERO_TABLE})")
print(f"Ground table:    {len(ground_table)} bytes  (max {CODE_START-GROUND_TABLE})")

assert len(alig_table)   <= HERO_TABLE   - ALIG_TABLE,   "Alligator table overflow"
assert len(hero_table)   <= GROUND_TABLE - HERO_TABLE,   "Hero table overflow"
assert len(ground_table) <= CODE_START   - GROUND_TABLE, "Ground table overflow"

BINARY_SIZE = 0x6000
program = bytearray(BINARY_SIZE)

# JP to code at 0x0000
program[0x0000] = 0xC3
program[0x0001] = CODE_START & 0xFF
program[0x0002] = (CODE_START >> 8) & 0xFF

# Background fill table (16384 bytes all BG_BYTE)
for i in range(16384):
    program[FILL_TABLE + i] = BG_BYTE

# Sprite and ground tables
for i, b in enumerate(alig_table):
    program[ALIG_TABLE + i] = b
for i, b in enumerate(hero_table):
    program[HERO_TABLE + i] = b
for i, b in enumerate(ground_table):
    program[GROUND_TABLE + i] = b

# ---------------------------------------------------------------------------
# Code assembly
# ---------------------------------------------------------------------------
ROW_COUNT_ADDR = 0x8000

code = bytearray()
def emit(b): code.extend(b)

def emit_masked_blit(table_addr, row_count, bytes_per_row):
    """
    Masked sprite blit.
    Reads interleaved table: [dest_lo, dest_hi, mask0, spr0, ...]
    Uses PUSH HL / POP HL to preserve table pointer across VRAM read.
    """
    # LD HL, table_addr
    emit([0x21, table_addr & 0xFF, (table_addr >> 8) & 0xFF])
    # Store row count in RAM
    emit([0x3E, row_count])
    emit([0x32, ROW_COUNT_ADDR & 0xFF, (ROW_COUNT_ADDR >> 8) & 0xFF])

    row_loop = len(code)
    emit([0x5E, 0x23])               # LD E,(HL) / INC HL
    emit([0x56, 0x23])               # LD D,(HL) / INC HL  → DE=VRAM dest
    emit([0x0E, bytes_per_row])      # LD C, bytes_per_row

    byte_loop = len(code)
    emit([0x46, 0x23])               # LD B,(HL) / INC HL  → B=mask
    emit([0x7E, 0x23])               # LD A,(HL) / INC HL  → A=sprite byte
    emit([0xE5])                     # PUSH HL (save table ptr)
    emit([0x6F])                     # LD L, A  (move sprite to L)
    emit([0x1A])                     # LD A,(DE) (read background)
    emit([0xA0])                     # AND B    (clear opaque positions)
    emit([0xB5])                     # OR L     (write sprite pixels)
    emit([0x12])                     # LD (DE),A (write to VRAM)
    emit([0xE1])                     # POP HL  (restore table ptr)
    emit([0x13])                     # INC DE
    emit([0x0D])                     # DEC C
    back = byte_loop - (len(code) + 2)
    emit([0x20, back & 0xFF])        # JR NZ, byte_loop

    # Decrement row counter in RAM
    emit([0x3A, ROW_COUNT_ADDR & 0xFF, (ROW_COUNT_ADDR >> 8) & 0xFF])
    emit([0x3D])                     # DEC A
    emit([0x32, ROW_COUNT_ADDR & 0xFF, (ROW_COUNT_ADDR >> 8) & 0xFF])
    back = row_loop - (len(code) + 2)
    emit([0x20, back & 0xFF])        # JR NZ, row_loop

def emit_ground_blit(table_addr, row_count):
    """
    Solid ground fill — no masking needed.
    Table format: [dest_lo, dest_hi, byte * GROUND_W] per row.
    """
    emit([0x21, table_addr & 0xFF, (table_addr >> 8) & 0xFF])
    emit([0x3E, row_count])
    emit([0x32, ROW_COUNT_ADDR & 0xFF, (ROW_COUNT_ADDR >> 8) & 0xFF])

    row_loop = len(code)
    emit([0x5E, 0x23])               # LD E,(HL) / INC HL
    emit([0x56, 0x23])               # LD D,(HL) / INC HL
    emit([0x0E, GROUND_W])           # LD C, 80

    byte_loop = len(code)
    emit([0x7E, 0x23])               # LD A,(HL) / INC HL
    emit([0x12])                     # LD (DE),A
    emit([0x13])                     # INC DE
    emit([0x0D])                     # DEC C
    back = byte_loop - (len(code) + 2)
    emit([0x20, back & 0xFF])        # JR NZ, byte_loop

    emit([0x3A, ROW_COUNT_ADDR & 0xFF, (ROW_COUNT_ADDR >> 8) & 0xFF])
    emit([0x3D])
    emit([0x32, ROW_COUNT_ADDR & 0xFF, (ROW_COUNT_ADDR >> 8) & 0xFF])
    back = row_loop - (len(code) + 2)
    emit([0x20, back & 0xFF])

# Phase 1: fill background with LDIR
emit([0x21, FILL_TABLE & 0xFF, (FILL_TABLE >> 8) & 0xFF])  # LD HL, FILL_TABLE
emit([0x11, 0x00, 0xC0])                                    # LD DE, 0xC000
emit([0x01, 0x00, 0x40])                                    # LD BC, 0x4000
emit([0xED, 0xB0])                                          # LDIR

# Phase 2: draw ground line
emit_ground_blit(GROUND_TABLE, GROUND_H)

# Phase 3: blit alligator (facing left)
emit_masked_blit(ALIG_TABLE, ALIG_H, ALIG_BYTES)

# Phase 4: blit hero (facing right)
emit_masked_blit(HERO_TABLE, HERO_H, HERO_BYTES)

# HALT
emit([0x76])

print(f"Code size: {len(code)} bytes")
assert CODE_START + len(code) <= BINARY_SIZE, "Code overflow"

for i, b in enumerate(code):
    program[CODE_START + i] = b

# ---------------------------------------------------------------------------
# Write binary
# ---------------------------------------------------------------------------
bin_path = "programs/lesson9.bin"
with open(bin_path, "wb") as f:
    f.write(program)

# ---------------------------------------------------------------------------
# Write ASM listing
# ---------------------------------------------------------------------------
asm_lines = [
    "; lesson9.asm  --  AUTO-GENERATED by gen_lesson9.py  --  DO NOT EDIT",
    "; WPS-Z80 Lesson 9: Sprite Blitting with AND mask transparency",
    ";",
    f"; Background: pen {BG_PEN} (dark blue)       fill byte 0x{BG_BYTE:02X}",
    f"; Ground:     pen {GROUND_PEN} (yellow-brown) at y={GROUND_Y}",
    f"; Alligator:  24x16 px, facing left,  byte col {ALIG_X} (px {ALIG_X*2}), row {ALIG_Y}",
    f"; Hero:       16x20 px, facing right, byte col {HERO_X} (px {HERO_X*2}), row {HERO_Y}",
    ";",
    "; Masked blit per byte pair:",
    ";   LD B,(HL) / INC HL   B = AND mask",
    ";   LD A,(HL) / INC HL   A = sprite byte",
    ";   PUSH HL               save table pointer",
    ";   LD L, A               move sprite to L",
    ";   LD A,(DE)             read background from VRAM",
    ";   AND B                 clear opaque pixel positions",
    ";   OR L                  write sprite pixels",
    ";   LD (DE),A             write back to VRAM",
    ";   POP HL                restore table pointer",
    ";   INC DE / DEC C / JR NZ",
]

asm_path = "programs/lesson9.asm"
with open(asm_path, "w") as f:
    f.write("\n".join(asm_lines) + "\n")

print(f"\nGenerated: {bin_path}  ({len(program)} bytes)")
print(f"Generated: {asm_path}")
print()
print("To run:")
print("  g++ main.cpp -o emulator.exe -std=c++17")
print("  python programs/gen_lesson9.py")
print("  .\\monitor.exe programs\\lesson9_vram --mode 0")
print("  .\\emulator.exe programs\\lesson9.bin")
print()
print("Expected:")
print("  - Dark blue background")
print("  - Yellow-brown ground line at bottom third of screen")
print("  - Alligator (green, facing LEFT, snout on left) right of centre")
print("  - Hero (red shirt, facing RIGHT) left of centre")
print("  - Clean transparency — no black cutout squares")