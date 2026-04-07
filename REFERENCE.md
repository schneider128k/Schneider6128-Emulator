# WPS-Z80 Technical Reference

**Revision:** 6.1 — Milestone 6 Stable
**Project:** Schneider CPC 6128 Emulation Layer
**Links:** [LOGBOOK.md](LOGBOOK.md) · [README.md](README.md)

---

## Architecture overview

![WPS-Z80 architecture diagram](docs/wps_z80_architecture.svg)

The emulator models a Zilog Z80 CPU connected to a flat 64 KB RAM space. The CPU fetches opcodes from the program area, executes them, and logs every step to a `.trace` file. The Branch Logic unit reads the Flag Register (F) to decide whether a conditional jump is taken. After execution, if any write touched `0xC000–0xFFFF`, the 16 KB VRAM block is dumped to a `.vram` file which the monitor reads and renders.

---

## Registers

| Register | Width | Role |
|----------|-------|------|
| PC | 16-bit | Program Counter. Points to the next opcode byte. Auto-increments on every fetch. |
| SP | 16-bit | Stack Pointer. Initialise to `0xF000`. Decrements on CALL, increments on RET. |
| A | 8-bit | Accumulator. Primary operand for arithmetic and logic. |
| B | 8-bit | Auxiliary register. Used as a loop counter in the `DEC B / JR NZ` idiom. |
| F | 8-bit | Flag Register. Status bits written by arithmetic ops, read by branch instructions. |

---

## Flag register (F)

Bit layout: `S Z — H — P/V N C` (MSB to LSB).

| Bit | Symbol | Name | Set when |
|-----|--------|------|----------|
| 7 | S | Sign | Result bit 7 is 1 (negative in signed arithmetic). |
| 6 | Z | Zero | Result is exactly zero. |
| 4 | H | Half-Carry | Carry from bit 3 into bit 4 (BCD; not yet used). |
| 2 | P/V | Parity / Overflow | Result parity or signed overflow (not yet used). |
| 1 | N | Add/Subtract | Last operation was a subtraction (not yet used). |
| 0 | C | Carry | Unsigned overflow on addition or borrow on subtraction/compare. |

Flag behaviour per instruction:

| Instruction | Z | C | S | Notes |
|-------------|---|---|---|-------|
| INC A | updated | — | updated | Z set if A wraps to 0x00. |
| DEC B | updated | — | updated | Z set when B reaches 0x00 — loop termination signal. |
| CP n | updated | updated | — | Silent A − n. Z set if equal; C set if A < n. |

---

## Instruction set

### System control

| Opcode | Mnemonic | Bytes | Description |
|--------|----------|-------|-------------|
| 0x00 | NOP | 1 | No operation. |
| 0x76 | HALT | 1 | Stop the execution loop. |

### 8-bit loads

| Opcode | Mnemonic | Bytes | Description |
|--------|----------|-------|-------------|
| 0x06 | LD B, n | 2 | Load immediate byte into B. |
| 0x3E | LD A, n | 2 | Load immediate byte into A. |
| 0x78 | LD A, B | 1 | Copy B into A. |
| 0x32 | LD (nn), A | 3 | Store A at absolute 16-bit address nn. |

### 16-bit loads

| Opcode | Mnemonic | Bytes | Description |
|--------|----------|-------|-------------|
| 0x31 | LD SP, nn | 3 | Load 16-bit immediate into SP. |

### Arithmetic and logic

| Opcode | Mnemonic | Bytes | Flags | Description |
|--------|----------|-------|-------|-------------|
| 0x3C | INC A | 1 | Z, S | Increment A. |
| 0x05 | DEC B | 1 | Z, S | Decrement B. |
| 0xFE | CP n | 2 | Z, C | Compare A with immediate n. Sets flags; result discarded. |

### Unconditional jumps

| Opcode | Mnemonic | Bytes | Description |
|--------|----------|-------|-------------|
| 0xC3 | JP nn | 3 | Absolute jump: PC ← nn. |
| 0x18 | JR e | 2 | Relative jump: PC ← PC + sign\_extend(e). |

### Conditional absolute jumps — JP cc, nn

The 16-bit target is always fetched. PC updates only when the condition is met.

| Opcode | Mnemonic | Condition | Bytes |
|--------|----------|-----------|-------|
| 0xC2 | JP NZ, nn | Z = 0 | 3 |
| 0xCA | JP Z, nn | Z = 1 | 3 |
| 0xD2 | JP NC, nn | C = 0 | 3 |
| 0xDA | JP C, nn | C = 1 | 3 |

### Conditional relative jumps — JR cc, e

The signed 8-bit displacement is always fetched. Added to PC only when the condition is met. Range: −126 to +129 bytes from the JR opcode address.

| Opcode | Mnemonic | Condition | Bytes |
|--------|----------|-----------|-------|
| 0x20 | JR NZ, e | Z = 0 | 2 |
| 0x28 | JR Z, e | Z = 1 | 2 |
| 0x30 | JR NC, e | C = 0 | 2 |
| 0x38 | JR C, e | C = 1 | 2 |

**Displacement encoding:** A backward branch of d bytes uses the two's-complement byte `256 − d`. Example: to jump back 3 bytes, encode `0xFD`.

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

---

## Memory map

| Range | Segment | Notes |
|-------|---------|-------|
| 0x0000 – 0x3FFF | Program area | Binary loaded here by the loader. Execution begins at 0x0000. |
| 0x4000 – 0x7FFF | General RAM | User data and variable storage. |
| 0x8000 – 0xBFFF | Working RAM | Register state dumps, inter-routine data. |
| 0xC000 – 0xFFFF | VRAM | Mode 1 screen buffer. Any write here sets `vram_dirty`. Dumped to `.vram` after execution. |
| 0xF000 – 0xFFFF | Stack segment | SP initialised to `0xF000`. Stack grows downward. Overlaps VRAM — do not use both simultaneously. |

---

## Video — Mode 1 (320×200, 4 colours)

### VRAM layout

The Gate Array reads 16 KB from `0xC000–0xFFFF`. The CRTC uses an 8-way interleave designed for character-cell text mode. For absolute pixel row `y` (0–199) and byte column `x` (0–79):
```
address = 0xC000 + (y % 8) * 0x0800 + (y / 8) * 0x0050 + x
```

Each row is 80 bytes wide (80 × 4 pixels = 320 pixels).

### Mode 1 byte encoding

One byte encodes 4 pixels. Each pixel has a 2-bit pen index (0–3). The bits are split across the byte:
```
bit7   bit6   bit5   bit4   bit3   bit2   bit1   bit0
p0[1]  p1[1]  p2[1]  p3[1]  p0[0]  p1[0]  p2[0]  p3[0]
```

To decode pixel n from byte b: `pen = ((b >> (7-n)) & 1) << 1 | ((b >> (3-n)) & 1)`

Solid-colour fill bytes (all 4 pixels the same pen):

| Pen | Binary | Hex |
|-----|--------|-----|
| 0 | 00000000 | 0x00 |
| 1 | 00001111 | 0x0F |
| 2 | 11110000 | 0xF0 |
| 3 | 11111111 | 0xFF |

### Default Mode 1 firmware palette

| Pen | Name | RGB |
|-----|------|-----|
| 0 | Black | (0, 0, 0) |
| 1 | Bright yellow | (255, 255, 0) |
| 2 | Bright cyan | (0, 255, 255) |
| 3 | Bright white | (255, 255, 255) |

---

## VRAM dump

After execution, if any write targeted `0xC000–0xFFFF`, `Memory::dumpVRAM()` writes the raw 16 KB block to:
```
programs/<name>_vram/frame_NNNN.vram
```

The folder is created automatically. The frame counter increments with each dump.

---

## Monitor

`monitor.cpp` compiles to a standalone `monitor.exe`. It watches a `_vram/` folder for new `.vram` files and renders each one as a live CPC Mode 1 frame. The monitor and emulator are completely independent — the only interface between them is the `_vram/` folder.

### SDL3 setup (Windows, one time only)

Download `SDL3-devel-3.4.4-mingw.tar.gz` from `github.com/libsdl-org/SDL/releases`. Extract it (requires 7-Zip for the `.tar.gz`). Then run these commands from the project root in PowerShell:
```powershell
Copy-Item -Recurse "SDL3-devel-3.4.4-mingw\SDL3-3.4.4\x86_64-w64-mingw32\include\SDL3" -Destination "SDL3\include\SDL3" -Force
New-Item -ItemType Directory -Force -Path "SDL3\lib"
Copy-Item "SDL3-devel-3.4.4-mingw\SDL3-3.4.4\x86_64-w64-mingw32\lib\libSDL3.dll.a" -Destination "SDL3\lib\" -Force
Copy-Item "SDL3-devel-3.4.4-mingw\SDL3-3.4.4\x86_64-w64-mingw32\bin\SDL3.dll" -Destination "." -Force
```

`SDL3/` and `SDL3.dll` are listed in `.gitignore` and are not committed to the repo.

### Build
```powershell
g++ monitor.cpp -o monitor.exe -I SDL3/include -L SDL3/lib -lSDL3 -std=c++17
g++ main.cpp -o emulator.exe -std=c++17
```

### Run

Open two terminals in VS Code (`Ctrl+Shift+`` ` opens a new terminal tab).

**Terminal 1 — launch the monitor first and leave it running:**
```powershell
.\monitor.exe programs\lesson6_vram
```

**Terminal 2 — run the emulator:**
```powershell
.\emulator.exe programs\lesson6.bin
```

The monitor window opens immediately showing a black screen. It updates within 200ms of the emulator finishing. If a `.vram` file already exists in the folder, the monitor renders it immediately on startup without needing to run the emulator again.

### Window

960×600 pixels (320×200 scaled ×3, nearest-neighbour — sharp CPC pixels). Title bar shows the current frame filename.

### Cross-platform notes

SDL3 uses DirectX 11/12 on Windows, Metal on macOS, and X11/Wayland on Linux. There is no platform-specific code in `monitor.cpp`. On Linux/macOS, install SDL3 via the system package manager and compile with `$(sdl3-config --cflags --libs)` instead of the manual `-I/-L` flags.

---

## Trace engine

Every executed instruction is written to `programs/<name>.trace`.

**Line format:**
```
[PPPP] MNEMONIC_AND_OPERANDS          | A:AA B:BB F:FFFFFFFF [Z:z C:c]
```

| Field | Description |
|-------|-------------|
| `[PPPP]` | PC value at the start of the instruction (hex). |
| Mnemonic | Disassembled instruction with operands and branch annotation. |
| `A:AA` | Accumulator (hex). |
| `B:BB` | Register B (hex). |
| `F:FFFFFFFF` | Full flag register as 8 binary digits, MSB first. |
| `[Z:z C:c]` | Zero and Carry flags isolated (0 or 1). |

Branch annotations:

| Annotation | Meaning |
|------------|---------|
| `[taken]` | Condition true; PC was updated. |
| `[not taken]` | Condition false; execution falls through. |
| `-> 0xNNNN` | Resolved destination for relative jumps (JR). |

**Safety limit:** Execution halts after `MAX_CYCLES` cycles (currently 5000). This guard must not be removed.

---

## Toolchain quick reference

**Build everything:**
```powershell
g++ main.cpp -o emulator.exe -std=c++17
g++ monitor.cpp -o monitor.exe -I SDL3/include -L SDL3/lib -lSDL3 -std=c++17
```

**Generate, run, and view a lesson:**
```powershell
python programs/gen_lessonN.py          # generates lessonN.bin + lessonN.asm
.\monitor.exe programs\lessonN_vram     # terminal 1 — launch first, leave running
.\emulator.exe programs\lessonN.bin     # terminal 2 — produces trace + vram dump
```

Each `gen_lessonN.py` is the single source of truth for its program. It produces both `lessonN.bin` (raw binary for the emulator) and `lessonN.asm` (human-readable annotated listing). Never edit `.asm` files by hand.

---

*(C) 1984 Wocjan Percussive Systems — "Binary precision / analog waves."*