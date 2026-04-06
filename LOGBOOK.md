# Development Logbook

## Lesson 2: Data Movers & The RL Bridge
**Date:** April 2026  
**Status:** 🏗️ In Progress

### 🧠 New Concepts
1. **8-bit Load Groups:** Implemented `LD B, n`, `LD A, B`, and `LD (nn), A`. These are the "sensors" and "actuators" that allow data to flow between CPU registers and RAM.
2. **Binary Decoupling:** Implemented a `loadFromFile` mechanism in the `Memory` class. The emulator now acts as a virtual machine, reading raw Z80 instructions from external files via command-line arguments.
3. **The "Reward Signal" Path:** Verified that we can "poke" values into specific RAM addresses (e.g., `0x7000`). In future RL iterations, this address will represent the game score, serving as the agent's reward signal.

### 💻 Technical Milestones
* **Binary Generation:** Successfully bypassed Windows shell limitations (`echo -ne` issues) by using Python to generate pure binary files.
* **Integrity Check:** Verified binary files using `Format-Hex`. Confirmed that while standard text viewers see ASCII artifacts (like `2x2p`), the underlying hex bytes (`06 32 78...`) are correctly aligned with Z80 opcodes.

### 📂 Supplemental Files
* [Technical Specification (PDF)](./schneider_6128_tech_spec_v1.pdf): Detailed hardware register and opcode reference.

---

## Lesson 1: The Heartbeat
**Date:** April 2026  
**Status:** ✅ Completed

### 🧠 Concepts Covered
1. **Z80 Architecture:** Defined the primary registers: Accumulator (A), Flags (F), and Program Counter (PC).
2. **The Fetch-Decode-Execute Cycle:** Implemented the core loop that reads an opcode from memory, increments the PC, and performs the logic.
3. **Little-Endian Memory:** Handled the Z80's specific way of reading 16-bit addresses (Low-byte first).
4. **Instruction Set (Opcode) Basics:**
   * `0x00` (NOP): No operation.
   * `0x3E` (LD A, n): Loading an immediate 8-bit value.
   * `0x3C` (INC A): Incrementing a register and updating Zero/Sign flags.
   * `0xC3` (JP nn): Unconditional jump to a memory address.

### 💻 Technical Milestones
* Created the `Memory` and `Z80CPU` structs in C++.
* Verified the core loop with a simple program that increments a value until it hits a jump instruction.
* Successfully pushed the initial codebase to GitHub.