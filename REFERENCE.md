# WPS-Z80 Technical Reference

**Revision:** 7.0 — Milestone 7 (Full Register Set + LDIR + All Three Video Modes)
**Project:** Schneider CPC 6128 Emulation Layer
**Links:** [LOGBOOK.md](LOGBOOK.md) · [README.md](README.md)

---

## Architecture overview

![WPS-Z80 architecture diagram](docs/wps_z80_architecture.svg)

The emulator models a Zilog Z80 CPU connected to a flat 64 KB RAM space. The CPU fetches opcodes from the program area, executes them, and logs every step to a `.trace` file. The Branch Logic unit reads the Flag Register (F) to decide whether conditional jumps are taken. After execution, if any write touched `0xC000–0xFFFF`, the 16 KB VRAM block is dumped to a `.vram` file for the monitor. The monitor decodes the VRAM using the selected video mode (0, 1, or 2) and renders it live.

---

## Registers

| Register | Width | Role |
|----------|-------|------|
| PC | 16-bit | Program Counter. Points to the next opcode byte. Auto-increments on every fetch. |
| SP | 16-bit | Stack Pointer. Initialise to `0xF000`. Decrements on CALL, increments on RET. |
| A | 8-bit | Accumulator. Primary operand for arithmetic and logic. |
| F | 8-bit | Flag Register. Written by arithmetic ops, read by branch instructions. |
| B, C | 8-bit | Pair BC. B = loop counter idiom. C = auxiliary. |
| D, E | 8-bit | Pair DE. Primary destination pointer (LDIR target). |
| H, L | 8-bit | Pair HL. Primary source/memory pointer (LDIR source, indirect addressing). |

**16-bit pair accessors** (computed from 8-bit halves, not stored separately):

| Accessor | Value | Role |
|----------|-------|------|
| BC() | (B<<8)\|C | Loop counter for LDIR/LDDR |
| DE() | (D<<8)\|E | Destination pointer |
| HL() | (H<<8)\|L | Source pointer / indirect address |

---

## Flag register (F)

Bit layout: `S Z — H — P/V N C` (MSB to LSB).

| Bit | Symbol | Name | Set when |
|-----|--------|------|----------|
| 7 | S | Sign | Result bit 7 is 1. |
| 6 | Z | Zero | Result is exactly zero. |
| 4 | H | Half-Carry | Carry from bit 3 into bit 4 (BCD; not yet used). |
| 2 | P/V | Parity / Overflow | P/V=0 when LDIR/LDDR completes (BC=0). |
| 1 | N | Add/Subtract | Last operation was a subtraction (not yet used). |
| 0 | C | Carry | Unsigned overflow on addition or borrow on subtraction/compare. |

Flag behaviour per instruction:

| Instruction | Z | C | S | P/V | Notes |
|-------------|---|---|---|-----|-------|
| INC A/B/C | updated | — | updated | — | Z set if result wraps to 0x00. |
| DEC B/C | updated | — | updated | — | Z set when register reaches 0x00. |
| CP n / CP B | updated | updated | — | — | Silent A − n. Z=equal, C=borrow. |
| LDIR / LDDR | — | — | — | updated | P/V=0 when BC reaches 0. |
| INC/DEC BC/DE/HL | — | — | — | — | 16-bit ops set no flags. |

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

### 8-bit loads — indirect via HL

| Opcode | Mnemonic | Bytes | Description |
|--------|----------|-------|-------------|
| 0x77 | LD (HL), A | 1 | Store A at address held in HL. |
| 0x36 | LD (HL), n | 2 | Store immediate byte at address in HL. |
| 0x7E | LD A, (HL) | 1 | Load A from address held in HL. |

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

### Arithmetic and logic

| Opcode | Mnemonic | Bytes | Flags | Description |
|--------|----------|-------|-------|-------------|
| 0x3C | INC A | 1 | Z, S | Increment A. |
| 0x04 | INC B | 1 | Z, S | Increment B. |
| 0x0C | INC C | 1 | Z, S | Increment C. |
| 0x05 | DEC B | 1 | Z, S | Decrement B. |
| 0x0D | DEC C | 1 | Z, S | Decrement C. |
| 0xFE | CP n | 2 | Z, C | Compare A with immediate n. Sets flags; result discarded. |
| 0xB8 | CP B | 1 | Z, C | Compare A with B. Sets flags; result discarded. |

### 16-bit increment / decrement (no flags affected)

| Opcode | Mnemonic | Bytes | Description |
|--------|----------|-------|-------------|
| 0x03 | INC BC | 1 | Increment BC. |
| 0x13 | INC DE | 1 | Increment DE. |
| 0x23 | INC HL | 1 | Increment HL. |
| 0x0B | DEC BC | 1 | Decrement BC. |
| 0x1B | DEC DE | 1 | Decrement DE. |
| 0x2B | DEC HL | 1 | Decrement HL. |

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

**Displacement encoding:** backward branch of d bytes → encode `256 − d`. Example: jump back 3 bytes → `0xFD`.

**Canonical counted loop:**
```asm
        LD B, N       ; initialise counter
LOOP:   DEC B         ; decrement; Z set when B reaches 0
        JR NZ, LOOP   ; branch back if Z = 0
```

### Stack and subroutines

| Opcode | Mnemonic | Bytes | Description |
|--------|----------|-------|-------------|
| 0xCD | CALL nn | 3 | Push PC onto stack (SP -= 2), then jump to nn. |
| 0xC9 | RET | 1 | Pop return address from stack into PC (SP += 2). |

### Extended opcodes — 0xED prefix

When `0xED` is fetched, a second byte is read and dispatched.

| Opcodes | Mnemonic | Bytes | Description |
|---------|----------|-------|-------------|
| ED B0 | LDIR | 2 | Copy (HL)→(DE); INC HL; INC DE; DEC BC; repeat until BC=0. Sets P/V=0. |
| ED B8 | LDDR | 2 | Copy (HL)→(DE); DEC HL; DEC DE; DEC BC; repeat until BC=0. Sets P/V=0. |

**Canonical VRAM fill using LDIR:**
```asm
LD HL, fill_table   ; source address
LD DE, 0xC000       ; destination: VRAM base
LD BC, 0x4000       ; count: 16384 bytes (full VRAM address space)
LDIR                ; block copy
HALT
```

---

## Memory map

| Range | Segment | Notes |
|-------|---------|-------|
| 0x0000 – 0x3FFF | Program area | Binary loaded here. Execution begins at 0x0000. |
| 0x4000 – 0x7FFF | General RAM | User data and variable storage. |
| 0x8000 – 0xBFFF | Working RAM | Register state dumps, inter-routine data. |
| 0xC000 – 0xFFFF | VRAM | 16 KB video buffer. Any write sets `vram_dirty`. Dumped after execution. |
| 0xF000 – 0xFFFF | Stack segment | SP initialised to `0xF000`. Grows downward. Overlaps VRAM — do not use both simultaneously. |

---

## Video modes

All three modes share the same CRTC addressing formula. The mode only affects how bytes are decoded into pixel colours.

### CRTC address formula (all modes)

For pixel row `y` (0–199) and byte column `x` (0–79):
```
address = 0xC000 + (y % 8) * 0x0800 + (y / 8) * 0x0050 + x
```

The VRAM address space is 16384 bytes. The CRTC interleave means maximum offset = `7 * 0x0800 + 24 * 0x0050 + 79 = 16335`, which exceeds 16000. Fill tables must be 16384 bytes, and LDIR counts must use `0x4000`.

### Mode 0 — 160×200, 16 colours

1 byte = 2 pixels, 4-bit pen index (0–15), scrambled bit layout:
```
bit7=p0[0] bit6=p1[0] bit5=p0[2] bit4=p1[2]
bit3=p0[1] bit2=p1[1] bit1=p0[3] bit0=p1[3]
```

Decode: `p0 = (b>>7&1) | (b>>5&1)<<1 | (b>>3&1)<<2 | (b>>1&1)<<3`

Solid pen fill byte: `(p[0]<<7)|(p[0]<<6)|(p[2]<<5)|(p[2]<<4)|(p[1]<<3)|(p[1]<<2)|(p[3]<<1)|p[3]`

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

### Run

Open two terminals in VS Code (`Ctrl+Shift+`` ` opens a new tab).

**Terminal 1 — launch monitor first, leave running:**
```powershell
.\monitor.exe programs\lessonN_vram --mode 0
.\monitor.exe programs\lessonN_vram --mode 1
.\monitor.exe programs\lessonN_vram --mode 2
```

**Terminal 2 — run emulator:**
```powershell
.\emulator.exe programs\lessonN.bin
```

The monitor renders within 200ms of the emulator finishing. If a `.vram` file already exists the monitor renders it immediately on startup.

### Window

960×600 pixels (320×200 scaled ×3, nearest-neighbour). Title bar shows mode and current frame filename.

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

**Safety limit:** Execution halts after `MAX_CYCLES` cycles (currently 5000). Must not be removed.

---

## Toolchain quick reference

**Build everything:**
```powershell
g++ main.cpp    -o emulator.exe -std=c++17
g++ monitor.cpp -o monitor.exe -I SDL3/include -L SDL3/lib -lSDL3 -std=c++17
```

**Generate, run, and view a lesson:**
```powershell
python programs/gen_lessonN.py
.\monitor.exe programs\lessonN_vram --mode 0    # terminal 1
.\emulator.exe programs\lessonN.bin              # terminal 2
```

Each `gen_lessonN.py` produces both `lessonN.bin` and `lessonN.asm`. Never edit `.asm` by hand.

---

*(C) 1984 Wocjan Percussive Systems — "Binary precision / analog waves."*