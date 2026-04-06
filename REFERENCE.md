# WOCJAN PERCUSSIVE SYSTEMS - Z80 CORE UNIT
## TECHNICAL REFERENCE MANUAL | REVISION 3.0
## PROJECT: SCHNEIDER 6128 EMULATION LAYER

---

### [01] ARCHITECTURAL OVERVIEW
The WPS-Z80 is an 8-bit micro-logic environment designed for high-precision execution and future neural-network integration.
The following schematic represents the logical mapping and signal flow between the CPU and the 64KB RAM environment.

```text
==============================================================================
                WPS-Z80 CORE UNIT (MILESTONE 3) - BUS DIAGRAM
==============================================================================
 [        Z80 CPU        ]                            [   MEMORY 64KB RAM   ]
 -------------------------                            -----------------------
 |   +-----------+       |                            |      0000h          |
 |   |    PC     | 16-bit|                            | PROGRAM / ROM AREA  |
 |   | (0x0000)  |=======|====[ ADDRESS BUS ]========>| (Entry Vector)      |
 |   +-----------+       |    (Unidirectional)        |                     |
 -------------------------                            -----------------------
 |   +-----------+       |                            |      4000h          |
 |   |    SP     | 16-bit|                            |   GENERAL DATA      |
 |   | (0xF000)  |=======|====[ ADDRESS BUS ]========>|   / USER RAM        |
 |   +-----------+       |    (Unidirectional)        |                     |
 -------------------------                            |                     |
 |   +-----------+       |                            |                     |
 |   |     A     | 8-bit |                            |                     |
 |   | (Accum.)  |=======|<===[  DATA BUS   ]========>|                     |
 |   +-----------+       |    (Bidirectional)         |                     |
 -------------------------                            |                     |
 |   +-----------+       |                            -----------------------
 |   |     B     | 8-bit |                            |      C000h          |
 |   | (Auxiliary) |=====|<===[  DATA BUS   ]========>|  RESERVED VRAM*     |
 |   +-----------+       |                            -----------------------
 |                       |                            |      F000h          |
 |   +-----------+       |                            | STACK SEGMENT**     |
 |   |  CONTROL  |       |                            | (Grows Downward)    |
 |   |  DECODE   |=======|===========================>|                     |
 |   +-----------+       |                            |      FFFFh          |
 -------------------------                            -----------------------
 NOTES:
 * C000h-C7FFh: VRAM Segment (Potential video/neural input layer).
 ** STACK OPERATION: CALL decrements SP by 2; RET increments SP by 2.
==============================================================================
```

#### REGISTER CONFIGURATION
* **A (Accumulator):** Primary 8-bit logic engine.
* **B (Auxiliary):** 8-bit high-speed storage.
* **SP (Stack Pointer):** 16-bit LIFO memory controller.
* **PC (Program Counter):** 16-bit sequence tracker.

---

### [02] VERIFIED INSTRUCTION SET (CORE 3.0)
The following opcodes are fully implemented and validated via the WPS-Trace Engine.

| MNEMONIC | OPCODE | BYTES | FUNCTIONAL DESCRIPTION |
| :--- | :--- | :--- | :--- |
| **LD A, n** | 3E | 2 | Load 8-bit Immediate to Accumulator. |
| **LD B, n** | 06 | 2 | Load 8-bit Immediate to Register B. |
| **LD A, B** | 78 | 1 | Transfer B to A. |
| **LD (nn), A**| 32 | 3 | Store Accumulator at Absolute Address `nn`. |
| **LD SP, nn** | 31 | 3 | Initialize Stack Pointer. |
| **INC A** | 3C | 1 | Increment Accumulator (A = A + 1). |
| **CALL nn** | CD | 3 | Subroutine Call (Push PC, Jump to `nn`). |
| **JP nn** | C3 | 3 | Unconditional Jump to Address `nn`. |
| **RET** | C9 | 1 | Subroutine Return (Pop PC from Stack). |
| **HALT** | 76 | 1 | Terminate Execution Loop. |
| **NOP** | 00 | 1 | Null Cycle. |

---

### [03] MEMORY & STACK DYNAMICS
* **MAPPING:** 64KB Linear RAM.
* **BOOT VECTOR:** Execution begins at `0x0000h`.
* **STACK OPERATION:** The Stack Pointer (SP) should be set to `0xF000h`. 
* **FLOW:** Each **CALL** decrements SP by 2. Each **RET** increments SP by 2. 

---

### [04] DIAGNOSTIC OUTPUT
Every execution cycle is recorded by the **WPS-Trace Engine**.
* **Trace Path:** `programs/[filename].trace`
* **Safety Protocol:** Automatic halt at 100 cycles to prevent rhythmic feedback loops.

---
(C) 1986 WOCJAN PERCUSSIVE SYSTEMS (WPS)
"BINARY PRECISION. ANALOG WAVES." 🥁🤘