# Schneider CPC 6128 Emulator (Z80 Core)

A high-performance C++ emulation of the classic Schneider/Amstrad CPC 6128 hardware. 

## 🎯 The Ultimate Goal: AI & Reinforcement Learning
Beyond simple emulation, the long-term vision for this project is to integrate the Z80 core into a **Reinforcement Learning framework (such as OpenAI Gym)**. 

By exposing the emulator's memory and video buffer to a Python-based RL agent, the goal is to recreate a **DeepMind-style DQN (Deep Q-Network)** experiment. The agent will "see" the emulated pixels and learn to master classic 8-bit games by interacting directly with this custom C++ engine.

## 🚧 Project Roadmap & Status

### Milestone 2: Data Movers & Binary Loading (Current)
* **Instruction Set:** Added 8-bit Load groups (`LD B, n`, `LD A, B`, `LD (nn), A`).
* **Binary Loader:** Implemented a decoupled system to load raw `.bin` files into RAM via command-line arguments.
* **RL Bridge:** Verified the ability to "poke" values into specific RAM addresses for reward-signal tracking.

### Milestone 1: The Heartbeat (Completed)
* **Core Architecture:** Defined Z80 registers (A, F, PC) and 64KB Memory Map.
* **Execution Loop:** Implemented the Fetch-Decode-Execute cycle and Little-Endian word fetching.
* **Basic Opcodes:** Support for `NOP`, `LD A, n`, `INC A`, and `JP nn`.

## 🛠️ Tech Stack
* **Language:** C++
* **Architecture:** Zilog Z80 (8-bit)
* **Build System:** Manual (g++ / clang)
* **Goal Environment:** OpenAI Gym / Stable Baselines 3

## 🔬 Personal Notes
This project is an evolving labor of love, developed between:
* Wrestling with the complex math of **Quantum Algorithms**.
* Mastering the drum fills of **The Pixies** ("Wave of Mutilation") and **Ghost** ("Square Hammer").