# Schneider CPC 6128 Emulator (Z80 Core)

> **Quick Links:** [Technical Reference](REFERENCE.md) | [Development Log](LOGBOOK.md) | [Programs](programs)

---

A C++17 emulation of the Zilog Z80 CPU targeting the Schneider/Amstrad CPC 6128 hardware, with a live SDL3 monitor for real-time video output.

## The Vision: AI & Reinforcement Learning

The goal of this project is to build a lightweight, modular CPC 6128 emulator that exposes the raw pixel buffer as an observation space for Reinforcement Learning agents. Rather than relying on hand-crafted features or game state variables, the agent learns entirely from pixels — the same visual information a human player would see. The emulator is designed to be wrapped in a Python Gymnasium environment, enabling standard deep RL algorithms such as DQN to discover game-playing strategies directly from screen frames.

## Project Architecture

- **Core:** C++17 Z80 CPU emulator (`main.cpp`) — 64KB RAM, full register set, ALU, LDIR block copy.
- **Monitor:** C++17 SDL3 live display (`monitor.cpp`) — decodes CPC Mode 0/1/2 VRAM and renders at 960×600.
- **Programs:** Python assembler scripts in `programs/` — each generates a `.bin` binary and `.asm` listing.
- **Tracing:** Every execution writes a `.trace` file for debugging and regression testing.
- **VRAM output:** Every execution dumps a `.vram` file to a `<name>_vram/` folder — the observation channel for the RL agent.

## How to Build
```powershell
g++ main.cpp    -o emulator.exe -std=c++17
g++ monitor.cpp -o monitor.exe -I SDL3/include -L SDL3/lib -lSDL3 -std=c++17
```

SDL3 setup (Windows, one time): see [REFERENCE.md](REFERENCE.md) for the exact PowerShell commands.

## How to Run

Each lesson requires two terminals. Open a second terminal in VS Code with `Ctrl+Shift+`` `.

**Step 1 — Generate the program:**
```powershell
python programs/gen_lesson8.py
```

**Step 2 — Launch the monitor first** (Terminal 1, leave running):
```powershell
.\monitor.exe programs\lesson8_vram --mode 0
```

**Step 3 — Run the emulator** (Terminal 2):
```powershell
.\emulator.exe programs\lesson8.bin
```

The monitor window updates within 200ms of the emulator finishing. If a `.vram` file already exists in the folder, the monitor renders it immediately on startup without needing to run the emulator again.

## Video Modes

The monitor supports all three authentic CPC screen modes via the `--mode` flag:

| Flag | Mode | Resolution | Colours | Use |
|------|------|------------|---------|-----|
| `--mode 0` | Mode 0 | 160×200 | 16 | Games and sprites (Uncle Alligator) |
| `--mode 1` | Mode 1 | 320×200 | 4 | Default — used in lessons 1–6 |
| `--mode 2` | Mode 2 | 640×200 | 2 | Text and UI overlays |

## Current Milestone

**Milestone 8.0 — Full ALU.** The emulator now has a complete arithmetic and logic unit including `ADD`, `SUB`, `AND`, `OR`, `XOR`, and 16-bit `ADD HL, rr`. Lesson 8 draws a filled rectangle computed entirely at runtime using address arithmetic — no precomputed pixel table.

## Development History

For detailed technical notes, opcode implementations, and lesson-by-lesson progress see [LOGBOOK.md](LOGBOOK.md). For the full instruction set reference, memory map, video mode encoding, and toolchain guide see [REFERENCE.md](REFERENCE.md).

## Roadmap

- **Milestone 9** — Sprite blitting: Uncle Alligator appears on screen for the first time.
- **Milestone 10** — Python RL hook: Gymnasium environment wrapping the emulator.
- **Future** — Full Schneider 6128 accurate emulator branch (sound, floppy, CRTC timing).