# 📔 Schneider 6128 Development Logbook

This logbook serves as a technical diary for the reconstruction of the Z80 CPU and the Schneider CPC 6128 hardware logic.

---

## Lesson 1: The Heartbeat (Fetch-Decode-Execute)
**Date:** April 2026  
**Status:** ✅ Completed

### 🧠 The Core Logic
Today, we transitioned from a static set of variables to a living **Fetch-Decode-Execute cycle**. This is the fundamental loop of all computing.

1. **The Memory Bus:** We simulated the physical copper traces on the Schneider motherboard using a C++ Pointer (`Memory* bus`). This allows the CPU to "reach out" to the RAM object.
   
2. **Instruction Fetching:** The Program Counter (`pc`) acts as a digital "finger" pointing at a specific memory address. Every time we call `fetch()`, the CPU reads the byte and moves the finger to the next position.

3. **Little-Endian Architecture:** We discovered that the Z80 (and the Schneider) expects 16-bit addresses "backwards" in memory. We implemented `fetchWord()` to grab the Low-byte first, then the High-byte, just like the real 1985 hardware.

4. **Conditionals & Flags:** We implemented the **Zero Flag**. This is how the CPU "remembers" if a calculation resulted in zero, allowing us to use `JP Z` (Jump if Zero) to break out of loops.

### 💻 Milestone Program
We successfully verified the logic by running a countdown loop in machine code:
- **`3E 03`** (LD A, 3)
- **`3D`** (DEC A)
- **`CA 09 00`** (JP Z to Finish)
- **`C3 02 00`** (JP back to DEC A)

**Result:** The CPU correctly decremented the accumulator and "teleported" the Program Counter to the end of the program once the Zero Flag was triggered.