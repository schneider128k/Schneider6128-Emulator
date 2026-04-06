#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <bitset>

/**
 * WOCJAN PERCUSSIVE SYSTEMS - Z80 CORE UNIT
 * Milestone 5: Branching (Conditional Jumps & Relative Jumps)
 * ----------------------------------------------------------------------------
 * ARCHITECTURE NOTE:
 * The Z80 uses the Flag Register (F) to store the results of arithmetic and
 * logical operations. This allows the CPU to perform conditional jumps.
 *   Bit 7: S (Sign)        - 1 if result is negative
 *   Bit 6: Z (Zero)        - 1 if result is zero
 *   Bit 4: H (Half-Carry)  - Used for BCD arithmetic
 *   Bit 2: P/V (Parity/Overflow) - Logic parity or signed overflow
 *   Bit 1: N (Add/Sub)     - 1 if last op was subtraction
 *   Bit 0: C (Carry)       - 1 if an unsigned overflow/underflow occurred
 * ----------------------------------------------------------------------------
 * Milestone 5 Additions (Branching):
 *   JP  nn      (0xC3) - Unconditional absolute jump [existed from Lesson 1]
 *   JP  NZ, nn  (0xC2) - Jump if Zero flag is CLEAR
 *   JP  Z,  nn  (0xCA) - Jump if Zero flag is SET
 *   JP  NC, nn  (0xD2) - Jump if Carry flag is CLEAR
 *   JP  C,  nn  (0xDA) - Jump if Carry flag is SET
 *   JR  e       (0x18) - Unconditional relative jump (signed 8-bit offset)
 *   JR  NZ, e   (0x20) - Relative jump if Zero flag is CLEAR
 *   JR  Z,  e   (0x28) - Relative jump if Zero flag is SET
 *   JR  NC, e   (0x30) - Relative jump if Carry flag is CLEAR
 *   JR  C,  e   (0x38) - Relative jump if Carry flag is SET
 * ----------------------------------------------------------------------------
 */

// WPS-Standard Flag Masks
const uint8_t FLAG_S = 0x80; // Bit 7
const uint8_t FLAG_Z = 0x40; // Bit 6
const uint8_t FLAG_H = 0x10; // Bit 4
const uint8_t FLAG_P = 0x04; // Bit 2
const uint8_t FLAG_N = 0x02; // Bit 1
const uint8_t FLAG_C = 0x01; // Bit 0

struct Memory {
    std::vector<uint8_t> ram;

    // Initialize 64KB of RAM (0x0000 to 0xFFFF)
    Memory() : ram(65536, 0) {}

    uint8_t read(uint16_t address) const { return ram[address]; }
    void write(uint16_t address, uint8_t value) { ram[address] = value; }

    // Load a binary file into memory starting at 0x0000
    bool loadFromFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) return false;
        file.read(reinterpret_cast<char*>(ram.data()), ram.size());
        return true;
    }
};

struct Z80CPU {
    // 8-bit Primary Registers
    uint8_t  a = 0;  // Accumulator
    uint8_t  b = 0;  // Auxiliary B
    uint8_t  f = 0;  // Flag Register

    // 16-bit Special Purpose Registers
    uint16_t pc = 0; // Program Counter
    uint16_t sp = 0; // Stack Pointer

    bool halted = false;

    // --- Utility Functions ---
    std::string to_hex(uint16_t val, int width = 4) {
        std::stringstream ss;
        ss << std::uppercase << std::hex << std::setw(width) << std::setfill('0') << val;
        return ss.str();
    }

    void log(std::ostream& os, const std::string& message) {
        std::cout << message << std::endl;
        os << message << "\n";
    }

    // Surgical Flag Manipulation
    void setFlag(uint8_t mask, bool condition) {
        if (condition) f |= mask;
        else           f &= ~mask;
    }

    // Flag query helpers (used by conditional branch logic)
    bool flagZ() const { return (f & FLAG_Z) != 0; }
    bool flagC() const { return (f & FLAG_C) != 0; }

    // --- Memory Interaction ---
    uint8_t fetch(Memory& mem) {
        return mem.read(pc++);
    }

    uint16_t fetch16(Memory& mem) {
        uint16_t low  = fetch(mem);
        uint16_t high = fetch(mem);
        return (high << 8) | low;
    }

    void push16(Memory& mem, uint16_t value) {
        sp--;
        mem.write(sp, (value >> 8) & 0xFF);
        sp--;
        mem.write(sp, value & 0xFF);
    }

    uint16_t pop16(Memory& mem) {
        uint8_t low  = mem.read(sp++);
        uint8_t high = mem.read(sp++);
        return (high << 8) | low;
    }

    // --- Milestone 5: Relative Jump Helper ---
    // JR uses a *signed* 8-bit displacement relative to the instruction
    // AFTER the JR opcode + displacement byte (i.e., relative to PC after fetch).
    // Cast to int8_t to sign-extend, then add to PC (which is already past the displacement).
    void do_jr(Memory& mem, bool condition, std::string& mnemonic, const std::string& tag) {
        int8_t offset = static_cast<int8_t>(fetch(mem)); // signed displacement
        if (condition) {
            pc = static_cast<uint16_t>(static_cast<int32_t>(pc) + offset);
            mnemonic = "JR " + tag + " -> 0x" + to_hex(pc);
        } else {
            mnemonic = "JR " + tag + " [not taken]";
        }
    }

    // --- Milestone 5: Absolute Conditional Jump Helper ---
    void do_jp_cond(Memory& mem, bool condition, std::string& mnemonic, const std::string& tag) {
        uint16_t target = fetch16(mem);
        if (condition) {
            pc = target;
            mnemonic = "JP " + tag + ", 0x" + to_hex(pc) + " [taken]";
        } else {
            mnemonic = "JP " + tag + ", 0x" + to_hex(target) + " [not taken]";
        }
    }

    // --- The Instruction Cycle ---
    void step(Memory& mem, std::ostream& trace) {
        if (halted) return;

        uint16_t current_pc = pc;
        uint8_t  opcode     = fetch(mem);
        std::string mnemonic;

        switch (opcode) {

        // === System Control ===
        case 0x00: // NOP
            mnemonic = "NOP";
            break;

        case 0x76: // HALT
            halted   = true;
            mnemonic = "HALT";
            break;

        // === 8-bit Loads ===
        case 0x3E: // LD A, n
            a = fetch(mem);
            mnemonic = "LD A, 0x" + to_hex(a, 2);
            break;

        case 0x06: // LD B, n
            b = fetch(mem);
            mnemonic = "LD B, 0x" + to_hex(b, 2);
            break;

        case 0x78: // LD A, B
            a = b;
            mnemonic = "LD A, B";
            break;

        case 0x32: { // LD (nn), A
            uint16_t addr = fetch16(mem);
            mem.write(addr, a);
            mnemonic = "LD (0x" + to_hex(addr) + "), A";
            break;
        }

        // === 16-bit Loads ===
        case 0x31: // LD SP, nn
            sp = fetch16(mem);
            mnemonic = "LD SP, 0x" + to_hex(sp);
            break;

        // === Arithmetic & Logic ===
        case 0x3C: // INC A
            a++;
            setFlag(FLAG_Z, a == 0);
            setFlag(FLAG_S, (a & 0x80) != 0);
            mnemonic = "INC A";
            break;

        case 0x05: // DEC B
            b--;
            setFlag(FLAG_Z, b == 0);
            setFlag(FLAG_S, (b & 0x80) != 0);
            mnemonic = "DEC B";
            break;

        case 0xFE: { // CP n
            uint8_t n      = fetch(mem);
            uint8_t result = a - n;
            setFlag(FLAG_Z, result == 0);
            setFlag(FLAG_C, a < n);
            mnemonic = "CP 0x" + to_hex(n, 2);
            break;
        }

        // === Jumps & Subroutines ===

        // --- Absolute Jumps ---
        case 0xC3: // JP nn  (unconditional, always present)
            pc = fetch16(mem);
            mnemonic = "JP 0x" + to_hex(pc);
            break;

        case 0xC2: // JP NZ, nn
            do_jp_cond(mem, !flagZ(), mnemonic, "NZ");
            break;

        case 0xCA: // JP Z, nn
            do_jp_cond(mem, flagZ(), mnemonic, "Z");
            break;

        case 0xD2: // JP NC, nn
            do_jp_cond(mem, !flagC(), mnemonic, "NC");
            break;

        case 0xDA: // JP C, nn
            do_jp_cond(mem, flagC(), mnemonic, "C");
            break;

        // --- Relative Jumps (JR) ---
        // JR uses a signed 8-bit offset from the byte AFTER the displacement.
        // This makes loops compact: the displacement fits in one byte,
        // and backward branches (loops) use negative offsets (0xFE = -2, etc.).

        case 0x18: { // JR e  (unconditional relative)
            int8_t offset = static_cast<int8_t>(fetch(mem));
            pc = static_cast<uint16_t>(static_cast<int32_t>(pc) + offset);
            mnemonic = "JR 0x" + to_hex(pc);
            break;
        }

        case 0x20: // JR NZ, e
            do_jr(mem, !flagZ(), mnemonic, "NZ");
            break;

        case 0x28: // JR Z, e
            do_jr(mem, flagZ(), mnemonic, "Z");
            break;

        case 0x30: // JR NC, e
            do_jr(mem, !flagC(), mnemonic, "NC");
            break;

        case 0x38: // JR C, e
            do_jr(mem, flagC(), mnemonic, "C");
            break;

        // --- Stack & Subroutines ---
        case 0xCD: { // CALL nn
            uint16_t target = fetch16(mem);
            mnemonic = "CALL 0x" + to_hex(target);
            push16(mem, pc);
            pc = target;
            break;
        }

        case 0xC9: // RET
            pc = pop16(mem);
            mnemonic = "RET";
            break;

        default:
            mnemonic = "UNKNOWN: 0x" + to_hex(opcode, 2);
            halted = true;
            break;
        }

        // --- Formatting the Trace Output ---
        std::stringstream ss;
        ss << "[" << to_hex(current_pc) << "] "
           << std::left << std::setw(30) << mnemonic
           << " | A:" << to_hex(a, 2) << " B:" << to_hex(b, 2)
           << " F:" << std::bitset<8>(f)
           << " [Z:" << (flagZ() ? '1' : '0') << " C:" << (flagC() ? '1' : '0') << "]";

        log(trace, ss.str());
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: ./emulator <programs/filename.bin>" << std::endl;
        return 1;
    }

    std::string binPath = argv[1];
    Memory  mem;
    Z80CPU  cpu;

    if (!mem.loadFromFile(binPath)) {
        std::cerr << "Error: Could not load " << binPath << std::endl;
        return 1;
    }

    // Auto-generate trace file name
    std::string tracePath = binPath;
    size_t lastDot = tracePath.find_last_of(".");
    if (lastDot != std::string::npos) tracePath = tracePath.substr(0, lastDot);
    tracePath += ".trace";

    std::ofstream traceFile(tracePath);
    if (!traceFile.is_open()) return 1;

    cpu.log(traceFile, "--- WPS-Z80 NEURAL TRACE: " + binPath + " ---");

    int cycles = 0;
    const int MAX_CYCLES = 100;

    while (!cpu.halted && cycles < MAX_CYCLES) {
        cpu.step(mem, traceFile);
        cycles++;
    }

    if (cycles >= MAX_CYCLES) cpu.log(traceFile, "--- Safety Limit Reached ---");
    cpu.log(traceFile, "--- Execution Finished ---");

    return 0;
}