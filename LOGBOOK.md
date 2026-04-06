# Schneider 6128 Development Log

**Current Milestone:** 4.0 (Flags & Comparative Logic)  
**Hardware Specification:** [WPS-Z80 Reference Manual](./REFERENCE.md)

---

## Lesson 5: The Decision Engine (Branching)

**Date:** April 2026
**Milestone:** 5.0 Stable
**Focus:** Conditional and relative jumps using the existing Flag Register.

### 🧠 New Concepts

* **JP cc, nn (Conditional Absolute Jump):** Four variants added — `JP NZ`, `JP Z`, `JP NC`, `JP C`. The CPU reads the 16-bit target address unconditionally, but only updates PC if the relevant flag matches.
* **JR e (Relative Jump):** Unconditional and four conditional variants (`JR NZ`, `JR Z`, `JR NC`, `JR C`). The displacement is a **signed 8-bit** value added to PC *after* the displacement byte is fetched. This makes backward loops compact (e.g., `JR NZ, -2` = `0x20 0xFE`).
* **Counted Loop Pattern:** `LD B, n` / `DEC B` / `JR NZ` — the canonical Z80 idiom for fixed-count loops. DEC B sets the Z flag when B hits 0, which is the loop termination condition.
* **Helper Methods:** Added `flagZ()` and `flagC()` inline queries to `Z80CPU` to keep branch logic readable and DRY.
* **Trace Enhancement:** Mnemonic column widened to 30 chars to accommodate conditional branch annotations (`[taken]` / `[not taken]`).

### 📂 Program Files

* [Source: gen_lesson5.py](programs/gen_lesson5.py)
* [Logic: lesson5.asm](programs/lesson5.asm)
* [Execution Trace: lesson5.trace](programs/lesson5.trace)

### ✅ Test Cases Passing

| Test | Expected Behavior |
|------|------------------|
| `JR NZ` loop | Executes body 3 times, exits when B=0 (Z set) |
| `CP 0xFF` + `JP Z` | Branch taken; `RAM[0x8000] = 0xFF` |
| Wrong-branch `HALT` | Never reached |
| All Lesson 1–4 bins | Unchanged; re-run confirms no regression |

---

## Lesson 4: The Decision Brainstem
**Date:** April 2026  
**Focus:** Comparative logic and the Flag Register (F).

### 🧠 New Concepts
* **Flag Register (F):** Introduced bit-level status monitoring for logic branching.
* **Zero Flag (Z):** Triggered when an operation result is zero; essential for equality checks.
* **Carry Flag (C):** Triggered by unsigned overflows or borrows during subtraction (A < n).
* **CP (Compare):** Implemented `CP n` (0xFE) to perform silent subtraction and update flags.

### 📂 Program Files
* [Source: gen_lesson4.py](./programs/gen_lesson4.py)
* [Logic: lesson4.asm](./programs/lesson4.asm)
* [Execution Trace: lesson4.trace](./programs/lesson4.trace)

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