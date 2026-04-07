# WPS-Z80 Technical Reference

**Revision:** 9.0 — Milestone 9 (Sprite Blitting)
**Project:** Schneider CPC 6128 Emulation Layer
**Links:** [LOGBOOK.md](LOGBOOK.md) · [README.md](README.md)

---

## Architecture overview

![WPS-Z80 architecture diagram](docs/wps_z80_architecture.svg)

The emulator models a Zilog Z80 CPU connected to a flat 64 KB RAM space. The CPU fetches opcodes from the program area, executes them, and logs every step to a `.trace` file. The Branch Logic unit reads the Flag Register (F) to decide whether conditional jumps are taken. After execution, if any write touched `0xC000–0xFFFF`, the 16 KB VRAM block is dumped to a `.vram` file for the monitor. The monitor decodes the VRAM using the selected video mode (0, 1, or 2) and renders it live at authentic CPC pixel proportions.

---

## Registers

| Register | Width | Role |
|----------|-------|------|
| PC | 16-bit | Program Counter. Points to the next opcode byte. Auto-increments on every fetch. |
| SP | 16-bit | Stack Pointer. Initialise to `0xF000`. Decrements on PUSH, increments on POP. |
| A | 8-bit | Accumulator. Primary operand for all ALU operations. |
| F | 8-bit | Flag Register. Written by ALU ops, read by branch instructions. |
| B, C | 8-bit | Pair BC. B = loop counter idiom. C = byte counter in blit loops. |
| D, E | 8-bit | Pair DE. Primary destination pointer (LDIR target, VRAM write). |
| H, L | 8-bit | Pair HL. Primary source/memory pointer (LDIR source, table pointer, indirect addressing). |

**16-bit pair accessors** (computed from 8-bit halves, not stored separately):

| Accessor | Value | Role |
|----------|-------|------|
| BC() | (B<<8)\|C | Loop counter for LDIR/LDDR |
| DE() | (D<<8)\|E | Destination pointer |
| HL() | (H<<8)\|L | Source pointer / indirect address / table pointer |

---

## Flag register (F)

Bit layout: `S Z — H — P/V N C` (MSB to LSB).

| Bit | Symbol | Name | Set when |
|-----|--------|------|----------|
| 7 | S | Sign | Result bit 7 is 1. |
| 6 | Z | Zero | Result is exactly zero. |
| 4 | H | Half-Carry | Carry from bit 3 into bit 4. Set by ADD and AND; cleared by OR/XOR. |
| 2 | P/V | Parity / Overflow | Parity of result for AND/OR/XOR (even parity = 1). P/V=0 when LDIR/LDDR completes. |
| 1 | N | Add/Subtract | Set by SUB/CP; cleared by ADD/AND/OR/XOR. |
| 0 | C | Carry | Unsigned overflow on ADD; borrow on SUB/CP. |

**Flag behaviour by instruction group:**

| Instruction | S | Z | H | P/V | N | C | Notes |
|-------------|---|---|---|-----|---|---|-------|
| ADD A, r/n | updated | updated | updated | — | 0 | updated | 8-bit add |
| SUB r/n | updated | updated | updated | — | 1 | updated | 8-bit subtract |
| CP r/n | updated | updated | updated | — | 1 | updated | Like SUB but A unchanged |
| AND r/n | updated | updated | 1 | parity | 0 | 0 | H always set |
| OR r/n | updated | updated | 0 | parity | 0 | 0 | H always cleared |
| XOR r/n | updated | updated | 0 | parity | 0 | 0 | H always cleared |
| INC r | updated | updated | updated | — | 0 | — | C not affected |
| DEC r | updated | updated | updated | — | 1 | — | C not affected |
| ADD HL, rr | — | — | updated | — | 0 | updated | 16-bit; S/Z not affected |
| LDIR / LDDR | — | — | 0 | 0 | 0 | — | P/V=0 when BC=0 |
| INC/DEC rr | — | — | — | — | — | — | 16-bit ops; no flags |

---

## Instruction set

### System control

| Opcode | Mnemonic | Bytes | Description |
|--------|----------|-------|-------------|
| 0x00 | NOP | 1 | No operation. |
| 0x76 | HALT | 1 | Stop the execution loop. |

### 8-bit loads — immediate

| Opcode | Mnemonic | Bytes | Description |
|--------|----------|-------|-------------|
| 0x3E | LD A, n | 2 | Load immediate into A. |
| 0x06 | LD B, n | 2 | Load immediate into B. |
| 0x0E | LD C, n | 2 | Load immediate into C. |
| 0x16 | LD D, n | 2 | Load immediate into D. |
| 0x1E | LD E, n | 2 | Load immediate into E. |
| 0x26 | LD H, n | 2 | Load immediate into H. |
| 0x2E | LD L, n | 2 | Load immediate into L. |

### 8-bit loads — register to register

| Opcode | Mnemonic | Bytes | Description |
|--------|----------|-------|-------------|
| 0x78 | LD A, B | 1 | Copy B into A. |
| 0x79 | LD A, C | 1 | Copy C into A. |
| 0x7A | LD A, D | 1 | Copy D into A. |
| 0x7B | LD A, E | 1 | Copy E into A. |
| 0x7C | LD A, H | 1 | Copy H into A. |
| 0x7D | LD A, L | 1 | Copy L into A. |
| 0x47 | LD B, A | 1 | Copy A into B. |
| 0x4F | LD C, A | 1 | Copy A into C. |
| 0x57 | LD D, A | 1 | Copy A into D. |
| 0x5F | LD E, A | 1 | Copy A into E. |
| 0x67 | LD H, A | 1 | Copy A into H. |
| 0x6F | LD L, A | 1 | Copy A into L. |

### 8-bit loads — HL indirect

| Opcode | Mnemonic | Bytes | Description |
|--------|----------|-------|-------------|
| 0x77 | LD (HL), A | 1 | Store A at address in HL. |
| 0x36 | LD (HL), n | 2 | Store immediate byte at address in HL. |
| 0x7E | LD A, (HL) | 1 | Load A from address in HL. |
| 0x46 | LD B, (HL) | 1 | Load B from address in HL. |
| 0x4E | LD C, (HL) | 1 | Load C from address in HL. |
| 0x56 | LD D, (HL) | 1 | Load D from address in HL. |
| 0x5E | LD E, (HL) | 1 | Load E from address in HL. |
| 0x66 | LD H, (HL) | 1 | Load H from address in HL. |
| 0x6E | LD L, (HL) | 1 | Load L from address in HL. |

### 8-bit loads — DE indirect

| Opcode | Mnemonic | Bytes | Description |
|--------|----------|-------|-------------|
| 0x12 | LD (DE), A | 1 | Store A at address in DE. |
| 0x1A | LD A, (DE) | 1 | Load A from address in DE. |

### 8-bit loads — absolute address

| Opcode | Mnemonic | Bytes | Description |
|--------|----------|-------|-------------|
| 0x32 | LD (nn), A | 3 | Store A at absolute 16-bit address nn. |
| 0x3A | LD A, (nn) | 3 | Load A from absolute 16-bit address nn. |

### 16-bit loads

| Opcode | Mnemonic | Bytes | Description |
|--------|----------|-------|-------------|
| 0x01 | LD BC, nn | 3 | Load 16-bit immediate into BC. |
| 0x11 | LD DE, nn | 3 | Load 16-bit immediate into DE. |
| 0x21 | LD HL, nn | 3 | Load 16-bit immediate into HL. |
| 0x31 | LD SP, nn | 3 | Load 16-bit immediate into SP. |

### 8-bit increment / decrement

| Opcode | Mnemonic | Bytes | Flags | Description |
|--------|----------|-------|-------|-------------|
| 0x3C | INC A | 1 | S, Z, H | Increment A. |
| 0x04 | INC B | 1 | S, Z, H | Increment B. |
| 0x0C | INC C | 1 | S, Z, H | Increment C. |
| 0x14 | INC D | 1 | S, Z, H | Increment D. |
| 0x1C | INC E | 1 | S, Z, H | Increment E. |
| 0x24 | INC H | 1 | S, Z, H | Increment H. |
| 0x2C | INC L | 1 | S, Z, H | Increment L. |
| 0x3D | DEC A | 1 | S, Z, H | Decrement A. |
| 0x05 | DEC B | 1 | S, Z, H | Decrement B. |
| 0x0D | DEC C | 1 | S, Z, H | Decrement C. |
| 0x15 | DEC D | 1 | S, Z, H | Decrement D. |
| 0x1D | DEC E | 1 | S, Z, H | Decrement E. |
| 0x25 | DEC H | 1 | S, Z, H | Decrement H. |
| 0x2D | DEC L | 1 | S, Z, H | Decrement L. |

### 16-bit increment / decrement (no flags affected)

| Opcode | Mnemonic | Bytes | Description |
|--------|----------|-------|-------------|
| 0x03 | INC BC | 1 | Increment BC. |
| 0x13 | INC DE | 1 | Increment DE. |
| 0x23 | INC HL | 1 | Increment HL. |
| 0x0B | DEC BC | 1 | Decrement BC. |
| 0x1B | DEC DE | 1 | Decrement DE. |
| 0x2B | DEC HL | 1 | Decrement HL. |

### ADD A, r/n — 8-bit addition

Flags: S, Z, H, C updated; N cleared.

| Opcode | Mnemonic | Bytes |
|--------|----------|-------|
| 0x87 | ADD A, A | 1 |
| 0x80 | ADD A, B | 1 |
| 0x81 | ADD A, C | 1 |
| 0x82 | ADD A, D | 1 |
| 0x83 | ADD A, E | 1 |
| 0x84 | ADD A, H | 1 |
| 0x85 | ADD A, L | 1 |
| 0xC6 | ADD A, n | 2 |

### SUB r/n — 8-bit subtraction

Flags: S, Z, H, C updated; N set.

| Opcode | Mnemonic | Bytes |
|--------|----------|-------|
| 0x97 | SUB A | 1 |
| 0x90 | SUB B | 1 |
| 0x91 | SUB C | 1 |
| 0x92 | SUB D | 1 |
| 0x93 | SUB E | 1 |
| 0xD6 | SUB n | 2 |

### CP r/n — compare (flags only, A unchanged)

Same flag behaviour as SUB.

| Opcode | Mnemonic | Bytes |
|--------|----------|-------|
| 0xFE | CP n | 2 |
| 0xB8 | CP B | 1 |
| 0xB9 | CP C | 1 |
| 0xBB | CP E | 1 |

### AND r/n — bitwise AND

Flags: S, Z, P updated; H=1; N=0; C=0.

| Opcode | Mnemonic | Bytes |
|--------|----------|-------|
| 0xA7 | AND A | 1 |
| 0xA0 | AND B | 1 |
| 0xA1 | AND C | 1 |
| 0xE6 | AND n | 2 |

### OR r/n — bitwise OR

Flags: S, Z, P updated; H=0; N=0; C=0.

| Opcode | Mnemonic | Bytes |
|--------|----------|-------|
| 0xB7 | OR A | 1 |
| 0xB0 | OR B | 1 |
| 0xB1 | OR C | 1 |
| 0xB2 | OR D | 1 |
| 0xB3 | OR E | 1 |
| 0xB4 | OR H | 1 |
| 0xB5 | OR L | 1 |
| 0xF6 | OR n | 2 |

### XOR r/n — bitwise XOR

Flags: S, Z, P updated; H=0; N=0; C=0.

`XOR A` (0xAF) is the canonical Z80 idiom to zero A and set Z=1 in a single byte.

| Opcode | Mnemonic | Bytes |
|--------|----------|-------|
| 0xAF | XOR A | 1 |
| 0xA8 | XOR B | 1 |
| 0xA9 | XOR C | 1 |
| 0xEE | XOR n | 2 |

### ADD HL, rr — 16-bit addition

Flags: H and C updated; N cleared. S and Z not affected.

| Opcode | Mnemonic | Bytes |
|--------|----------|-------|
| 0x09 | ADD HL, BC | 1 |
| 0x19 | ADD HL, DE | 1 |
| 0x29 | ADD HL, HL | 1 |
| 0x39 | ADD HL, SP | 1 |

### Stack — PUSH and POP

PUSH decrements SP by 2 then writes the register pair high byte first.
POP reads two bytes from SP and increments SP by 2.

| Opcode | Mnemonic | Bytes | Description |
|--------|----------|-------|-------------|
| 0xF5 | PUSH AF | 1 | Push A and F onto stack. |
| 0xF1 | POP AF | 1 | Pop A and F from stack. |
| 0xC5 | PUSH BC | 1 | Push BC onto stack. |
| 0xC1 | POP BC | 1 | Pop BC from stack. |
| 0xD5 | PUSH DE | 1 | Push DE onto stack. |
| 0xD1 | POP DE | 1 | Pop DE from stack. |
| 0xE5 | PUSH HL | 1 | Push HL onto stack. |
| 0xE1 | POP HL | 1 | Pop HL from stack. |

**Canonical register preservation during blit:**

```asm
PUSH HL        ; save table pointer
LD   L, A      ; move sprite byte to L
LD   A, (DE)   ; read background from VRAM
AND  B         ; apply AND mask
OR   L         ; apply sprite pixels
LD   (DE), A   ; write back to VRAM
POP  HL        ; restore table pointer
```

### Unconditional jumps

| Opcode | Mnemonic | Bytes | Description |
|--------|----------|-------|-------------|
| 0xC3 | JP nn | 3 | Absolute jump: PC ← nn. |
| 0x18 | JR e | 2 | Relative jump: PC ← PC + sign\_extend(e). |

### Conditional absolute jumps — JP cc, nn

| Opcode | Mnemonic | Condition | Bytes |
|--------|----------|-----------|-------|
| 0xC2 | JP NZ, nn | Z = 0 | 3 |
| 0xCA | JP Z, nn | Z = 1 | 3 |
| 0xD2 | JP NC, nn | C = 0 | 3 |
| 0xDA | JP C, nn | C = 1 | 3 |

### Conditional relative jumps — JR cc, e

| Opcode | Mnemonic | Condition | Bytes |
|--------|----------|-----------|-------|
| 0x20 | JR NZ, e | Z = 0 | 2 |
| 0x28 | JR Z, e | Z = 1 | 2 |
| 0x30 | JR NC, e | C = 0 | 2 |
| 0x38 | JR C, e | C = 1 | 2 |

**Displacement encoding:** backward branch of d bytes → encode `256 − d`.

**Canonical counted loop:**

```asm
       LD B, N       ; initialise counter
LOOP:   DEC B         ; Z set when B reaches 0
       JR NZ, LOOP   ; branch back if Z = 0
```

**Canonical blit inner loop:**

```asm
BYTE:   LD B, (HL)    ; AND mask
       INC HL
       LD A, (HL)    ; sprite byte
       INC HL
       PUSH HL       ; save table pointer
       LD L, A       ; move sprite to L
       LD A, (DE)    ; read background
       AND B         ; clear opaque positions
       OR  L         ; write sprite pixels
       LD (DE), A    ; write to VRAM
       POP HL        ; restore table pointer
       INC DE
       DEC C
       JR NZ, BYTE
```

### Subroutines

| Opcode | Mnemonic | Bytes | Description |
|--------|----------|-------|-------------|
| 0xCD | CALL nn | 3 | Push PC onto stack (SP -= 2), then jump to nn. |
| 0xC9 | RET | 1 | Pop return address from stack into PC (SP += 2). |

### Extended opcodes — 0xED prefix

| Opcodes | Mnemonic | Bytes | Description |
|---------|----------|-------|-------------|
| ED B0 | LDIR | 2 | Copy (HL)→(DE); INC HL; INC DE; DEC BC; repeat until BC=0. |
| ED B8 | LDDR | 2 | Copy (HL)→(DE); DEC HL; DEC DE; DEC BC; repeat until BC=0. |

**Canonical full VRAM fill:**

```asm
LD HL, fill_table   ; source
LD DE, 0xC000       ; destination: VRAM base
LD BC, 0x4000       ; count: 16384 bytes (full VRAM address space)
LDIR
```

---

## Memory map

| Range | Segment | Notes |
|-------|---------|-------|
| 0x0000 – 0x3FFF | Program area | Binary loaded here. Execution begins at 0x0000. |
| 0x4000 – 0x7FFF | General RAM | User data, fill tables, sprite tables, address tables. |
| 0x8000 – 0xBFFF | Working RAM | RAM scratch variables (e.g. row counter at 0x8000). |
| 0xC000 – 0xFFFF | VRAM | 16 KB video buffer. Any write sets `vram_dirty`. Dumped after execution. |
| 0xF000 – 0xFFFF | Stack segment | SP initialised to `0xF000`. Grows downward. Overlaps VRAM — do not use both simultaneously. |

---

## Sprite system

### Mode 0 pixel encoding

Two pixels per byte. Pen index is 4 bits (0–15), stored in a scrambled bit layout:

```
bit7=p0[0] bit6=p1[0] bit5=p0[2] bit4=p1[2]
bit3=p0[1] bit2=p1[1] bit1=p0[3] bit0=p1[3]
```

Encode pixel pair `(pl, pr)`:
```python
byte = (pl[0]<<7)|(pr[0]<<6)|(pl[2]<<5)|(pr[2]<<4)|(pl[1]<<3)|(pr[1]<<2)|(pl[3]<<1)|pr[3]
```

Decode pixel 0: `pen = (b>>7&1) | ((b>>3&1)<<1) | ((b>>5&1)<<2) | ((b>>1&1)<<3)`

**Verified solid pen fill bytes:**

| Pen | Byte | Pen | Byte | Pen | Byte | Pen | Byte |
|-----|------|-----|------|-----|------|-----|------|
| 0 | 0x00 | 4 | 0x30 | 8 | 0x03 | 12 | 0x33 |
| 1 | 0xC0 | 5 | 0xF0 | 9 | 0xC3 | 13 | 0xF3 |
| 2 | 0x0C | 6 | 0x3C | 10 | 0x0F | 14 | 0x3F |
| 3 | 0xCC | 7 | 0xFC | 11 | 0xCF | 15 | 0xFF |

### AND mask computation

For transparent pen `tp` (default pen 0):

```python
def mode0_mask(pl, pr, tp=0):
   lt = (pl == tp); rt = (pr == tp)
   mask = 0
   if lt: mask |= (1<<7)|(1<<5)|(1<<3)|(1<<1)
   if rt: mask |= (1<<6)|(1<<4)|(1<<2)|(1<<0)
   return mask
```

Both transparent → mask `0xFF` (keep all background).
Both opaque → mask `0x00` (clear all background before OR).

### Interleaved blit table format

Per sprite row (W bytes wide):
```
[dest_lo, dest_hi, mask_0, spr_0, mask_1, spr_1, ..., mask_(W-1), spr_(W-1)]
```
Total bytes per row: `2 + W*2`

| Sprite | Dimensions | Bytes/row | Rows | Table size |
|--------|------------|-----------|------|------------|
| Alligator | 24×16 px (12 bytes wide) | 26 | 16 | 416 bytes |
| Hero | 16×20 px (8 bytes wide) | 18 | 20 | 360 bytes |

### Sprite memory layout (lesson9)

| Address | Contents |
|---------|----------|
| 0x0100 | Background fill table (16384 bytes, solid pen 1) |
| 0x4200 | Alligator interleaved blit table (416 bytes) |
| 0x4400 | Hero interleaved blit table (360 bytes) |
| 0x4600 | Ground fill table (328 bytes, solid pen 12) |
| 0x4800 | Z80 code |
| 0x8000 | RAM scratch: row counter (1 byte) |

### Sprite orientation

Sprites are designed facing right in the Python pixel grids. `mirror_sprite(grid)` applies `row[::-1]` to each row to produce a left-facing version. Alligator frames are mirrored (faces left on screen). Hero frames are used as-is (faces right on screen).

---

## Video modes

All three modes share the same CRTC addressing formula. The mode only affects how bytes are decoded into pixel colours.

### CRTC address formula (all modes)

For pixel row `y` (0–199) and byte column `x` (0–79):

```
address = 0xC000 + (y % 8) * 0x0800 + (y / 8) * 0x0050 + x
```

Maximum offset = `7 * 0x0800 + 24 * 0x0050 + 79 = 16335`. Fill tables and LDIR counts must use `0x4000` (16384), not `0x3E80` (16000).

### Mode 0 — 160×200, 16 colours

See sprite system section above for full encoding details.

**Default Mode 0 firmware palette (16 pens):**

| Pen | Name | RGB |
|-----|------|-----|
| 0 | Black | (0, 0, 0) |
| 1 | Blue | (0, 0, 128) |
| 2 | Bright blue | (0, 0, 255) |
| 3 | Red | (128, 0, 0) |
| 4 | Magenta | (128, 0, 128) |
| 5 | Mauve | (128, 0, 255) |
| 6 | Bright red | (255, 0, 0) |
| 7 | Bright magenta | (255, 0, 128) |
| 8 | Bright mauve | (255, 0, 255) |
| 9 | Green | (0, 128, 0) |
| 10 | Cyan | (0, 128, 128) |
| 11 | Sky blue | (0, 128, 255) |
| 12 | Yellow | (128, 128, 0) |
| 13 | White (grey) | (128, 128, 128) |
| 14 | Pastel blue | (128, 128, 255) |
| 15 | Bright green | (0, 255, 0) |

### Mode 1 — 320×200, 4 colours

1 byte = 4 pixels, 2-bit pen index (0–3):

```
bit7=p0[1] bit6=p1[1] bit5=p2[1] bit4=p3[1]
bit3=p0[0] bit2=p1[0] bit1=p2[0] bit0=p3[0]
```

Decode pixel n: `pen = ((b>>(7-n))&1)<<1 | ((b>>(3-n))&1)`

Solid pen fill bytes: pen 0 = `0x00`, pen 1 = `0x0F`, pen 2 = `0xF0`, pen 3 = `0xFF`.

**Default Mode 1 firmware palette (4 pens):**

| Pen | Name | RGB |
|-----|------|-----|
| 0 | Black | (0, 0, 0) |
| 1 | Bright yellow | (255, 255, 0) |
| 2 | Bright cyan | (0, 255, 255) |
| 3 | Bright white | (255, 255, 255) |

### Mode 2 — 640×200, 2 colours

1 byte = 8 pixels, 1-bit pen index (0–1), MSB first:

`pen_n = (byte >> (7-n)) & 1`

**Default Mode 2 firmware palette (2 pens):**

| Pen | Name | RGB |
|-----|------|-----|
| 0 | Black | (0, 0, 0) |
| 1 | Bright white | (255, 255, 255) |

---

## VRAM dump

After execution, if any write targeted `0xC000–0xFFFF`, `Memory::dumpVRAM()` writes the raw 16 KB block to:

```
programs/<name>_vram/frame_NNNN.vram
```

The folder is created automatically. The frame counter increments with each dump.

---

## Monitor

`monitor.cpp` compiles to `monitor.exe`. Watches a `_vram/` folder and renders each new `.vram` file as a live CPC frame. Completely independent from the emulator — the only interface is the `_vram/` folder.

### Authentic CPC pixel aspect ratio

The Schneider CPC 6128 displays Mode 0 pixels at 2:1 width-to-height on a 4:3 monitor. The monitor reproduces this with `SCALE_X=6` / `SCALE_Y=3`, giving each logical pixel a 6×3 block of screen pixels. `SDL_SetRenderScale` is not used — `render_frame` draws explicit `SDL_FRect` rectangles in raw screen coordinates so x and y can scale independently.

Window size: 960×600 pixels (160×6 = 960 wide, 200×3 = 600 tall).

### SDL3 setup (Windows, one time only)

Download `SDL3-devel-3.4.4-mingw.tar.gz` from `github.com/libsdl-org/SDL/releases`. Extract with 7-Zip. Then in PowerShell from the project root:

```powershell
Copy-Item -Recurse "SDL3-devel-3.4.4-mingw\SDL3-3.4.4\x86_64-w64-mingw32\include\SDL3" -Destination "SDL3\include\SDL3" -Force
New-Item -ItemType Directory -Force -Path "SDL3\lib"
Copy-Item "SDL3-devel-3.4.4-mingw\SDL3-3.4.4\x86_64-w64-mingw32\lib\libSDL3.dll.a" -Destination "SDL3\lib\" -Force
Copy-Item "SDL3-devel-3.4.4-mingw\SDL3-3.4.4\x86_64-w64-mingw32\bin\SDL3.dll" -Destination "." -Force
```

`SDL3/` and `SDL3.dll` are in `.gitignore` — not committed.

### Build

```powershell
g++ main.cpp    -o emulator.exe -std=c++17
g++ monitor.cpp -o monitor.exe -I SDL3/include -L SDL3/lib -lSDL3 -std=c++17
```

### Run (single terminal)

```powershell
g++ main.cpp -o emulator.exe -std=c++17
g++ monitor.cpp -o monitor.exe -I SDL3/include -L SDL3/lib -lSDL3 -std=c++17
python programs\gen_lessonN.py
Remove-Item -Recurse -Force programs\lessonN_vram -ErrorAction SilentlyContinue
Start-Process -FilePath ".\monitor.exe" -ArgumentList "programs\lessonN_vram --mode 0"
.\emulator.exe programs\lessonN.bin
```

If the monitor window is already open, omit the `Start-Process` line — the existing window picks up the new `.vram` file automatically within 200ms.

### Window

960×600 pixels. Authentic CPC 6:3 pixel aspect ratio. Title bar shows mode and current frame filename.

### Cross-platform

SDL3 uses DirectX on Windows, Metal on macOS, X11/Wayland on Linux. No platform-specific code in `monitor.cpp`. On Linux/macOS compile with `$(sdl3-config --cflags --libs)`.

---

## Trace engine

**Line format:**

```
[PPPP] MNEMONIC                        | A:AA BC:BBBB DE:DDDD HL:HHHH F:FFFFFFFF [Z:z C:c]
```

| Field | Description |
|-------|-------------|
| `[PPPP]` | PC at start of instruction (hex). |
| Mnemonic | Disassembled instruction with operands and branch annotation. |
| `A:AA` | Accumulator (hex). |
| `BC:BBBB` | BC pair (hex). |
| `DE:DDDD` | DE pair (hex). |
| `HL:HHHH` | HL pair (hex). |
| `F:FFFFFFFF` | Full flag register, 8 binary digits MSB first. |
| `[Z:z C:c]` | Zero and Carry flags isolated. |

Branch annotations: `[taken]`, `[not taken]`, `-> 0xNNNN`.

**Safety limit:** Execution halts after `MAX_CYCLES` cycles (currently 50000). Must not be removed.

---

## Toolchain quick reference

**Build everything:**

```powershell
g++ main.cpp    -o emulator.exe -std=c++17
g++ monitor.cpp -o monitor.exe -I SDL3/include -L SDL3/lib -lSDL3 -std=c++17
```

**Generate, run, and view a lesson (single terminal):**

```powershell
python programs\gen_lessonN.py
Remove-Item -Recurse -Force programs\lessonN_vram -ErrorAction SilentlyContinue
Start-Process -FilePath ".\monitor.exe" -ArgumentList "programs\lessonN_vram --mode 0"
.\emulator.exe programs\lessonN.bin
```

Each `gen_lessonN.py` produces both `lessonN.bin` and `lessonN.asm`. Never edit `.asm` by hand.

---

*(C) 1984 Wocjan Percussive Systems — "Binary precision / analog waves."*