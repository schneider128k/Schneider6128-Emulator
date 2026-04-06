# Schneider 6128 Development Log

**Current Milestone:** 6.0 (Monitor Output)
**Hardware Specification:** [WPS-Z80 Reference Manual](REFERENCE.md)

---

## Lesson 6: The Monitor

**Date:** April 2026
**Focus:** VRAM output and live display using SDL3.

### 🧠 New Concepts

- **VRAM dirty flag:** `Memory::write()` now sets `vram_dirty` whenever an address in `0xC000–0xFFFF` is written. After execution the 16 KB block is dumped to a `.vram` file.
- **VRAM folder:** Each program gets its own `<name>_vram/` folder alongside its `.bin` file. Frames are written as `frame_0001.vram`, `frame_0002.vram`, etc.
- **Mode 1 encoding:** 1 byte encodes 4 pixels. Bit 1 of each pen index sits in the high nibble, bit 0 in the low nibble: `p0[1] p1[1] p2[1] p3[1] | p0[0] p1[0] p2[0] p3[0]`. Solid pen bytes: pen 0 = `0x00`, pen 1 = `0x0F`, pen 2 = `0xF0`, pen 3 = `0xFF`.
- **CRTC interleaved addressing:** The CPC does not lay out rows top-to-bottom linearly. For pixel row `y` (0–199) and byte column `x` (0–79): `address = 0xC000 + (y % 8) * 0x0800 + (y / 8) * 0x0050 + x`.
- **SDL3 monitor:** `monitor.cpp` compiles to a standalone `monitor.exe`. It watches the `_vram/` folder, decodes each new `.vram` file, and renders it into a 960×600 SDL3 window (320×200 scaled 3×) using nearest-neighbour filtering for sharp CPC pixels. Cross-platform: Win32/DirectX on Windows, Metal on Mac, X11/Wayland on Linux — no platform-specific code in `monitor.cpp`.
- **Authentic palette:** The four default Mode 1 firmware pens: black `(0,0,0)`, bright yellow `(255,255,0)`, bright cyan `(0,255,255)`, bright white `(255,255,255)`.

### 📂 Program Files

- [Source: gen\_lesson6.py](programs/gen_lesson6.py)
- [Logic: lesson6.asm](programs/lesson6.asm)
- [Monitor: monitor.cpp](monitor.cpp)

### ✅ Test

Expected display: four horizontal stripes each 50 rows tall — black (top), bright yellow, bright cyan, bright white (bottom). Confirms correct CRTC address decoding and Mode 1 pixel encoding.

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