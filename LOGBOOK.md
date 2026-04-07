# Schneider 6128 Development Log

**Current Milestone:** 9.0 (Sprite Blitting)
**Hardware Specification:** [WPS-Z80 Reference Manual](REFERENCE.md)

---

## Lesson 9: Uncle Alligator Appears

**Date:** April 2026
**Focus:** Side-view sprite blitting with AND mask transparency and authentic CPC pixel aspect ratio.

### 🧠 New Concepts

- **Masked sprite blitting:** Each sprite byte is paired with an AND mask byte. The blit sequence reads background from VRAM, ANDs with the mask (clearing opaque pixel positions), ORs in the sprite byte, and writes back. This gives clean transparency — pen 0 pixels in the sprite are invisible and the background shows through.
- **Interleaved table format:** Per sprite row: `[dest_lo, dest_hi, mask0, spr0, mask1, spr1, ..., mask(W-1), spr(W-1)]`. This keeps the VRAM destination address and sprite data together in a single linear block, avoiding the need for a separate pointer register. The blit loop reads them sequentially with `LD E,(HL)` / `LD D,(HL)` for the address, then `LD B,(HL)` / `LD A,(HL)` for each mask/sprite pair.
- **PUSH HL / POP HL:** The masked blit needs to read the background byte from VRAM via DE while HL holds the table pointer. Since we cannot use HL for two things simultaneously, we save HL onto the stack with `PUSH HL`, do the VRAM read and composite, then restore with `POP HL`. This is the first use of the stack for register preservation rather than subroutine calls.
- **Mode 0 mask computation:** For each pixel pair `(pl, pr)`, the mask bits are 1 where a pixel is transparent (pen 0) and 0 where opaque. Left pixel occupies bits 7,5,3,1; right pixel occupies bits 6,4,2,0. If both pixels are transparent the mask byte is `0xFF` (keep all background). If both are opaque the mask is `0x00` (clear all background before OR).
- **Side-view sprite design:** Alligator is 24 logical pixels wide × 16 tall (12 bytes × 16 rows). Hero is 16 logical pixels wide × 20 tall (8 bytes × 20 rows). Four alligator frames (2 walk poses × 2 mouth states) and 3 hero frames (run A, run B, jump) designed and encoded. Lesson 9 blits frame 1 of each.
- **Sprite orientation via mirroring:** Alligator pixel grids are drawn facing right internally, then `mirror_sprite()` (Python `row[::-1]`) flips each row to produce the left-facing sprite. Hero grids are drawn natively facing right. This keeps the pixel design intuitive while producing the correct screen orientation.
- **Ground line:** A 4-row solid fill at y=148 using pen 12 (yellow-brown, `0x33`) drawn via a simple write loop — no masking needed for solid fills. Provides a clear visual horizon for both the player and the RL agent.
- **Authentic CPC pixel aspect ratio:** The Schneider CPC 6128 displays Mode 0 pixels at 2:1 width-to-height ratio on a 4:3 monitor. `monitor.cpp` now uses `SCALE_X=6` / `SCALE_Y=3` with manual `SDL_FRect` drawing (no `SDL_SetRenderScale`) so x and y can scale independently. Result: 960×600 window with pixels that are 6×3 screen pixels — matching what the game would have looked like on real 1985 hardware.
- **Mode 0 decode bug fixed:** The original decoder had bits 3 and 5 swapped in the pen index reconstruction. Correct formula: `pens[0] = (b>>7&1) | ((b>>3&1)<<1) | ((b>>5&1)<<2) | ((b>>1&1)<<3)`. This was discovered when pen 12 (yellow-brown ground) was rendering as pen 10 (cyan).

### 🔧 Engineering Notes

- Alligator interleaved table: 16 rows × 26 bytes = 416 bytes at `0x4200`.
- Hero interleaved table: 20 rows × 18 bytes = 360 bytes at `0x4400`.
- Ground table: 4 rows × 82 bytes = 328 bytes at `0x4600`.
- Code at `0x4800`, RAM scratch (row counter) at `0x8000`.
- `MAX_CYCLES = 50000` is sufficient — the blit loops complete well within this limit.
- New opcodes used: `PUSH HL` (0xE5), `POP HL` (0xE1), `OR L` (0xB5), `LD B,(HL)` (0x46).

### 📂 Program Files

- [Source: gen\_lesson9.py](programs/gen_lesson9.py)
- [Logic: lesson9.asm](programs/lesson9.asm)

### ✅ Verified Output

Dark blue background (pen 1), yellow-brown ground line (pen 12) at y=148, alligator sprite (green, facing left, 24×16 px) at byte col 34 row 132, hero sprite (red shirt, facing right, 16×20 px) at byte col 8 row 128. Clean transparency on both sprites. Authentic CPC 6:3 pixel aspect ratio. Rendered at 960×600.

---

## Lesson 8: The ALU

**Date:** April 2026
**Focus:** Full arithmetic and logic unit — runtime address arithmetic and rectangle drawing.

### 🧠 New Concepts

- **ADD A, r/n:** 8-bit addition into A. Sets S, Z, H, C; clears N. Used for address offset computation and pixel value arithmetic.
- **SUB r/n:** 8-bit subtraction from A. Sets S, Z, H, C; sets N. Note: `SUB A` always produces zero with Z=1, C=0 — a fast way to clear A while setting flags.
- **AND r/n:** Bitwise AND with A. Sets S, Z, P (parity); H=1; clears N, C. Essential for masking pen bits and extracting pixel values from VRAM bytes.
- **OR r/n:** Bitwise OR with A. Sets S, Z, P; clears H, N, C. Used for compositing pixel data.
- **XOR r/n:** Bitwise XOR with A. Sets S, Z, P; clears H, N, C. `XOR A` (opcode `0xAF`) is the canonical Z80 idiom for zeroing A and setting Z=1 in a single byte — every real CPC program uses it.
- **ADD HL, rr:** 16-bit addition into HL. Updates C and clears N only — does not touch S or Z. Used for advancing VRAM row pointers by one character line stride (`0x0050`) or one pixel row stride (`0x0800`).
- **Parity flag (P/V):** Now correctly computed for AND/OR/XOR operations. P=1 if the number of set bits in the result is even.
- **ALU flag helpers:** `flags_add()`, `flags_sub()`, `flags_and()`, `flags_or_xor()` — factored out of the switch to keep flag logic DRY and auditable.
- **LD (DE), A / LD A, (DE):** Store and load via DE pointer. Used in the rectangle fill loop alongside `INC DE`.
- **LD r, (HL) for all registers:** `LD D, (HL)` and `LD E, (HL)` added (previously missing), completing the full set of indirect loads from HL into any 8-bit register.
- **MAX_CYCLES raised to 50000:** The rectangle fill loop is 80 rows × 40 bytes × 4 instructions = 12,800 steps. Previous limit of 5000 was insufficient.

### 🔧 Engineering Notes

- `SUB A` had a subtle self-reference bug: `flags_sub` reads `a` as the minuend to compute carry, but the operand is also `a`. Fixed by saving `old = a` before the call.
- The rectangle address table (80 × 2 bytes) at `0x4100` precomputes CRTC row addresses for the rectangle rows. The main loop reads these with `LD E, (HL)` / `LD D, (HL)` / `INC HL` pairs — the first real use of runtime pointer dereferencing into DE.

### 📂 Program Files

- [Source: gen\_lesson8.py](programs/gen_lesson8.py)
- [Logic: lesson8.asm](programs/lesson8.asm)

### ✅ Verified Output

Green screen (pen 9) with a bright red rectangle (pen 6) drawn at runtime using address arithmetic. No precomputed pixel table for the rectangle — CRTC addresses computed from a row address table, pixels written via `LD (DE), A` loop.

---

## Lesson 7: Full Screen

**Date:** April 2026
**Focus:** Full register set, LDIR block copy, and all three CPC video modes.

### 🧠 New Concepts

- **Complete register file:** Added C, D, E, H, L completing the three 16-bit pairs BC, DE, HL. Pairs are accessed via computed accessors `BC()`, `DE()`, `HL()` and set via `setBC()`, `setDE()`, `setHL()`. The pair values are derived from the 8-bit halves — not stored separately — which is architecturally correct and means `INC H` and `INC HL` affect the same underlying bytes.
- **HL as memory pointer:** HL is the Z80's primary indirect addressing register. `LD (HL), A` stores A at the address held in HL. `LD A, (HL)` loads A from that address. `INC HL` advances the pointer to the next byte. This is the foundation for all sprite blitting and screen filling.
- **LDIR (ED B0):** Load, Increment, Repeat. Copies BC bytes from (HL) to (DE), incrementing both pointers and decrementing BC each iteration until BC reaches zero. In one instruction it fills the entire 16 KB VRAM in a single Z80 operation — what previously required 16,000 individual `LD (nn), A` instructions now takes one `LDIR`. This is the canonical CPC screen-clearing instruction.
- **LDDR (ED B8):** Same as LDIR but decrements HL and DE — copies a block backwards. Useful for overlapping memory regions.
- **0xED prefix:** The extended opcode family. When the emulator fetches `0xED` it reads a second byte and dispatches again. This prefix unlocks a large family of Z80 instructions that cannot fit in the single-byte opcode space.
- **Mode 0 video (160×200, 16 colours):** The primary mode for Uncle Alligator and game development. One byte encodes 2 pixels with a scrambled 4-bit pen index. The bit layout is the most complex of the three modes. Default firmware palette has 16 colours from the CPC's 27-colour 3-level RGB palette.
- **Mode 2 video (640×200, 2 colours):** Highest resolution mode. One byte encodes 8 pixels at 1 bit each (MSB first). Useful for text and UI overlays.
- **`--mode N` flag:** The monitor now accepts `--mode 0`, `--mode 1`, or `--mode 2` as a command-line argument. Default is mode 1 for backward compatibility with Lesson 6.
- **VRAM fill table bug:** The CRTC interleave means some VRAM offsets exceed 16000. The fill table must be 16384 bytes (full VRAM address space), and `LD BC` must be `0x4000` not `0x3E80`. Fixed in gen_lesson7.py.

### 📂 Program Files

- [Source: gen\_lesson7.py](programs/gen_lesson7.py)
- [Logic: lesson7.asm](programs/lesson7.asm)
- [Monitor: monitor.cpp](monitor.cpp)

### ✅ Verified Output

Full 960×600 monitor window filled edge to edge with 16 horizontal colour bands in Mode 0, each approximately 12–13 rows tall. All 16 firmware palette colours visible. No black lines or gaps.

---

## Lesson 6: The Monitor

**Date:** April 2026
**Focus:** VRAM output and live SDL3 display.

### 🧠 New Concepts

- **VRAM dirty flag:** `Memory::write()` sets `vram_dirty` on any write to `0xC000–0xFFFF`. After execution the full 16 KB block is dumped to disk.
- **VRAM folder convention:** Each program gets its own `<name>_vram/` folder alongside its `.bin` file. Frames are named `frame_0001.vram`, `frame_0002.vram`, etc. The folder is created automatically by the emulator.
- **Mode 1 byte encoding:** 1 byte = 4 pixels, 2 bits per pixel. Bit 1 of each pen index sits in the high nibble, bit 0 in the low nibble. Solid pen fill bytes: pen 0 = `0x00`, pen 1 = `0x0F`, pen 2 = `0xF0`, pen 3 = `0xFF`.
- **CRTC interleaved addressing:** The CPC lays out pixel rows in an 8-way interleave, not top-to-bottom linearly. For pixel row `y` (0–199) and byte column `x` (0–79): `address = 0xC000 + (y % 8) * 0x0800 + (y / 8) * 0x0050 + x`. Verified by direct byte inspection of the `.vram` dump.
- **SDL3 monitor:** `monitor.cpp` compiles to a standalone `monitor.exe`. It watches a `_vram/` folder, decodes each new `.vram` file using the CRTC formula and Mode 1 decoder, and renders it into a 960×600 window (320×200 scaled ×3). Uses `SDL_SetRenderScale` + `SDL_SetRenderDrawColor` + `SDL_RenderFillRect` — SDL3 handles all colour format translation internally with no platform-specific pixel format constants.
- **Authentic CPC palette:** Default Mode 1 firmware pens: black `(0,0,0)`, bright yellow `(255,255,0)`, bright cyan `(0,255,255)`, bright white `(255,255,255)`.
- **MAX_CYCLES raised to 5000:** Lesson 6 writes 200 rows × 9 instructions = 1800 instructions. The previous limit of 1000 was too low and caused the white stripe to be missing. Raised to 5000 to accommodate VRAM-writing programs.

### 🔧 Engineering Notes

- Initial monitor implementation used `SDL_CreateTexture` with `SDL_PIXELFORMAT_RGBA8888`. This produced incorrect colours (magenta, wrong channel order) because SDL3's native pixel format on Windows does not match the byte order we write. Fixed by switching to direct renderer drawing: `SDL_SetRenderScale` sets the 3× scale factor, `SDL_SetRenderDrawColor` sets the colour, and `SDL_RenderFillRect` draws each CPC pixel as a 1×1 logical unit that SDL3 scales internally.
- The VRAM formula was verified correct by inspecting raw bytes at key offsets: row 0 offset `0x0000` = `0x00`, row 50 offset `0x11E0` = `0x0F`, row 100 offset `0x23C0` = `0xF0`, row 150 offset `0x35A0` = `0xFF`.

### 📂 Program Files

- [Source: gen\_lesson6.py](programs/gen_lesson6.py)
- [Logic: lesson6.asm](programs/lesson6.asm)
- [Monitor: monitor.cpp](monitor.cpp)

### ✅ Verified Output

Four horizontal stripes visible in the monitor window, each 50 rows tall. From top to bottom: black, bright yellow, bright cyan, bright white. Stripes cover the left 32 pixels of each row (8 bytes × 4 pixels). The rest of the screen is black — full-screen fill requires `LDIR` (Milestone 7).

---

## Lesson 5: The Decision Engine

**Date:** April 2026
**Focus:** Conditional and relative jumps using the existing Flag Register.

### 🧠 New Concepts

- **JP cc, nn (Conditional Absolute Jump):** Four variants — `JP NZ`, `JP Z`, `JP NC`, `JP C`. The 16-bit target is always fetched; PC updates only if the condition is met.
- **JR e (Relative Jump):** Unconditional and four conditional variants (`JR NZ`, `JR Z`, `JR NC`, `JR C`). The signed 8-bit displacement is added to PC after the displacement byte is fetched. Backward branches use two's-complement encoding.
- **Counted loop pattern:** `LD B, n` / `DEC B` / `JR NZ` — the canonical Z80 idiom for fixed-count loops.
- **Helper methods:** Added `flagZ()` and `flagC()` inline queries to keep branch logic readable.

### 📂 Program Files

- [Source: gen\_lesson5.py](programs/gen_lesson5.py)
- [Logic: lesson5.asm](programs/lesson5.asm)
- [Execution Trace: lesson5.trace](programs/lesson5.trace)

---

## Lesson 4: The Decision Brainstem

**Date:** April 2026
**Focus:** Comparative logic and the Flag Register (F).

### 🧠 New Concepts

- **Flag Register (F):** Introduced bit-level status monitoring for logic branching.
- **Zero Flag (Z):** Triggered when an operation result is zero; essential for equality checks.
- **Carry Flag (C):** Triggered by unsigned overflows or borrows during subtraction (A < n).
- **CP (Compare):** Implemented `CP n` (0xFE) to perform silent subtraction and update flags.

### 📂 Program Files

- [Source: gen\_lesson4.py](programs/gen_lesson4.py)
- [Logic: lesson4.asm](programs/lesson4.asm)
- [Execution Trace: lesson4.trace](programs/lesson4.trace)

---

## Lesson 3: The Stack & Subroutines

**Date:** April 2026
**Focus:** Non-linear execution and the Trace Engine.

### 🧠 New Concepts

- **The Stack Pointer (SP):** Implemented as a 16-bit register. The stack grows downward in memory.
- **CALL/RET:** The CPU pushes the return address onto the stack before jumping; RET pops it back into PC.
- **Auto-Tracing:** Every execution writes a `.trace` file co-located with the `.bin` program.

### 📂 Program Files

- [Source: gen\_lesson3.py](programs/gen_lesson3.py)
- [Logic: lesson3.asm](programs/lesson3.asm)
- [Execution Trace: lesson3.trace](programs/lesson3.trace)

---

## Lesson 2: Binary Loading & Memory Pokes

**Date:** April 2026
**Focus:** Decoupled binary loader and memory store instructions.

### 🧠 New Concepts

- **Binary Loader:** Switched from hardcoded arrays to a `.bin` file loader. Enables clean regression testing after every update.
- **Instruction Set:** Added `LD B, n`, `LD A, B`, `LD (nn), A`.

### 📂 Program Files

- [Source: gen\_lesson2.py](programs/gen_lesson2.py)

---

## Lesson 1: The Heartbeat

**Date:** April 2026
**Focus:** Core fetch-decode-execute loop.

### 🧠 New Concepts

- **Core Architecture:** Defined registers (A, F, PC) and the Fetch-Decode-Execute loop.
- **Opcodes:** `NOP`, `LD A, n`, `INC A`, `JP nn`.

### 📂 Program Files

- [Source: gen\_lesson1.py](programs/gen_lesson1.py)