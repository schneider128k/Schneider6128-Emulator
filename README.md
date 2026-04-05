# 🕹️ Schneider CPC 6128 Emulator (Z80 Core)

Welcome to my digital reconstruction of the 1985 Schneider (Amstrad) CPC 6128. This is a passion project focused on simulating the Zilog Z80 microprocessor from the ground up in C++.

## 🚧 Status: Work in Progress
This project is an evolving labor of love. I tend to push updates to the CPU logic whenever I'm not:
* 🔬 Wrestling with the complex math of **Quantum Algorithms**.
* 🥁 Getting up to speed with the drum fills of **"Wave of Mutilation" by the Pixies** or locked into the groove of **"Square Hammer" by Ghost**.

If the commit history looks like a drum fill—fast, erratic, but hopefully in time—now you know why!

## 🚀 Current Milestone: The "Heartbeat"
The emulator has officially "come to life." We have successfully implemented the basic **Fetch-Decode-Execute** cycle. 

**Current Capabilities:**
* **Memory Mapping:** Full 64KB RAM addressing with a simulated Bus.
* **Instruction Set:** Core opcodes like `LD` (Load), `INC/DEC` (Math), and `JP` (Jumps).
* **Logic:** A working Zero Flag that allows the CPU to handle loops and basic "decision making."

## 🛠️ Tech Stack
* **Language:** C++ (Standard 17+)
* **Toolchain:** MSYS2 / MinGW-w64
* **Editor:** VS Code (with C++ Runner extension)

## 📚 How to Run
If you want to see the Z80 "think," clone the repo and run:
```bash
g++ main.cpp -o emulator.exe
./emulator.exe