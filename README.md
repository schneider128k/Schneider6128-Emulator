# Schneider CPC 6128 Emulator (Z80 Core)

A educational C++ reconstruction of the classic 1985 Schneider (Amstrad) CPC 6128 home computer. This project focuses on simulating the Zilog Z80 microprocessor and its interaction with 64KB of system RAM.

## 🚀 Current Status: Milestone 1
We have successfully implemented the **Fetch-Decode-Execute** cycle. The emulator can currently:
* Address a full 64KB Memory Map.
* Perform 8-bit Arithmetic (INC/DEC).
* Handle Control Flow (Unconditional and Conditional Jumps).
* Manage CPU Flags (Zero and Sign flags).

## 🛠️ How to Set Up
This project uses the **MSYS2 (UCRT64)** toolchain on Windows.

### Prerequisites
1. Install [VS Code](https://code.visualstudio.com/).
2. Install the **C/C++ Extension** (Microsoft) and **C/C++ Runner** (franneck94).
3. Install **MSYS2** and the `mingw-w64-ucrt-x86_64-toolchain`.

### Building and Running
Open the folder in VS Code and use the "Run" button provided by the C++ Runner, or use the terminal:
```bash
g++ main.cpp -o emulator.exe
./emulator.exe