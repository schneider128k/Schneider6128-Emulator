# Schneider CPC 6128 Emulator (Z80 Core)

> **Quick Links:** [Technical Reference](./REFERENCE.md) | [Development Log](./LOGBOOK.md) | [Programs](./programs/)

---

A high-performance C++ emulation of the Zilog Z80 CPU, specifically targeting the Schneider/Amstrad CPC 6128 hardware.

## 🚀 The Vision: AI & Reinforcement Learning
The goal of this project is to create a lightweight, modular emulator that can be wrapped in a **Python OpenAI Gym environment**. This will allow Reinforcement Learning agents to "play" 8-bit games by processing the video buffer and memory states.

## 🏗️ Project Architecture
* **Core:** C++17 (Z80 instruction set, 64KB RAM).
* **Toolchain:** Python-based "Assembler" scripts in `/programs`.
* **Tracing:** Every execution generates a `.trace` file for headless debugging and GitHub visibility.

## 🛠️ How to Run
1. **Compile:** `g++ main.cpp -o emulator`
2. **Generate Program:** `python programs/gen_lesson3.py`
3. **Execute:** `.\emulator.exe programs/lesson3.bin`

## 📖 Development History
For detailed technical notes, opcode implementations, and lesson-by-lesson progress, see the [LOGBOOK.md](./LOGBOOK.md).