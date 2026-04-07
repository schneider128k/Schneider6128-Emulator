# Schneider 6128 Development Log

**Current Milestone:** 6.0 (Monitor Output — Stable)
**Hardware Specification:** [WPS-Z80 Reference Manual](REFERENCE.md)

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