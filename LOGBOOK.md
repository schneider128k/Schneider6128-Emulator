# Schneider 6128 Development Log

**Current Milestone:** 3.0 (Stack & Subroutines)  
**Hardware Specification:** [WPS-Z80 Reference Manual](./REFERENCE.md)

---

## Lesson 3: The Stack & Subroutines
**Date:** April 2026  
**Focus:** Non-linear execution and the Trace Engine.

### 🧠 New Concepts
* **The Stack Pointer (SP):** Implemented as a 16-bit register. The stack grows **downward** in memory.
* **CALL/RET:** The "Heart" of subroutines. The CPU now pushes the return address to the stack before jumping.
* **Auto-Tracing:** Implemented a `.trace` file generator to log every instruction for GitHub review.

### 📂 Program Files
* [Source: gen_lesson3.py](./programs/gen_lesson3.py)
* [Logic: lesson3.asm](./programs/lesson3.asm)
* [Execution Trace: lesson3.trace](./programs/lesson3.trace)

---

## Lesson 2: Binary Loading & Memory Pokes
**Status:** ✅ Completed
* **Loader:** Switched from hardcoded arrays to a decoupled `.bin` file loader.
* **Instruction Set:** Added 8-bit load groups (`LD B, n`, `LD A, B`, `LD (nn), A`).
* [Source: gen_lesson2.py](./programs/gen_lesson2.py)

---

## Lesson 1: The Heartbeat
**Status:** ✅ Completed
* **Core Architecture:** Defined registers (A, F, PC) and the Fetch-Decode-Execute loop.
* **Opcodes:** `NOP`, `LD A, n`, `INC A`, `JP nn`.
* [Source: gen_lesson1.py](./programs/gen_lesson1.py)