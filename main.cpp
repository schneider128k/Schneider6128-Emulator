#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <cstdint>
#include <iomanip>
#include <sstream>

/**
 * Schneider CPC 6128 Emulator - Z80 Core
 * Milestone 3: Stack Pointer, Subroutines, and Auto-Tracing
 * * ARCHITECTURE NOTE: 
 * The Z80 is a Little-Endian processor. 16-bit words are stored 
 * as (Low Byte, High Byte) in memory. The Stack grows DOWNWARD, 
 * meaning PUSH decrements the Stack Pointer (SP).
 */

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
    // 8-bit Registers
    uint8_t a = 0; // Accumulator
    uint8_t b = 0; // General Purpose B

    // 16-bit Special Purpose Registers
    uint16_t pc = 0; // Program Counter
    uint16_t sp = 0; // Stack Pointer

    bool halted = false;

    // --- Helper Functions ---

    // Utility to format numbers as hex strings for the trace log
    std::string to_hex(uint16_t val, int width = 4) {
        std::stringstream ss;
        ss << std::uppercase << std::hex << std::setw(width) << std::setfill('0') << val;
        return ss.str();
    }

    // Logs messages to both the standard console and the .trace file
    void log(std::ostream& os, const std::string& message) {
        std::cout << message << std::endl;
        os << message << "\n";
    }

    // Fetch a single byte from memory and advance the Program Counter
    uint8_t fetch(Memory& mem) {
        return mem.read(pc++);
    }

    // Fetch a 16-bit word (Little-Endian) from memory
    uint16_t fetch16(Memory& mem) {
        uint16_t low = fetch(mem);
        uint16_t high = fetch(mem);
        return (high << 8) | low;
    }

    // PUSH a 16-bit value onto the Stack (Decrements SP)
    void push16(Memory& mem, uint16_t value) {
        sp--; // Move down for High Byte
        mem.write(sp, (value >> 8) & 0xFF);
        sp--; // Move down for Low Byte
        mem.write(sp, value & 0xFF);
    }

    // POP a 16-bit value from the Stack (Increments SP)
    uint16_t pop16(Memory& mem) {
        uint8_t low = mem.read(sp++);  // Read Low, then move up
        uint8_t high = mem.read(sp++); // Read High, then move up
        return (high << 8) | low;
    }

    // --- Main Execution Step ---
    void step(Memory& mem, std::ostream& trace) {
        if (halted) return;

        uint16_t current_pc = pc; // Store PC before fetching for logging
        uint8_t opcode = fetch(mem);
        std::string msg;

        switch (opcode) {
            
            // === Group 1: System Control ===
            case 0x00: // NOP (No Operation)
                msg = "[0x" + to_hex(current_pc) + "] NOP";
                break;

            case 0x76: // HALT (Stop CPU execution)
                halted = true;
                msg = "[0x" + to_hex(current_pc) + "] HALT";
                break;

            // === Group 2: 8-bit Load Operations ===
            case 0x3E: // LD A, n (Immediate)
                a = fetch(mem);
                msg = "[0x" + to_hex(current_pc) + "] LD A, 0x" + to_hex(a, 2);
                break;

            case 0x06: // LD B, n (Immediate)
                b = fetch(mem);
                msg = "[0x" + to_hex(current_pc) + "] LD B, 0x" + to_hex(b, 2);
                break;

            case 0x78: // LD A, B (Register to Register)
                a = b;
                msg = "[0x" + to_hex(current_pc) + "] LD A, B";
                break;

            case 0x32: { // LD (nn), A (Store Accumulator to Memory)
                uint16_t addr = fetch16(mem);
                mem.write(addr, a);
                msg = "[0x" + to_hex(current_pc) + "] LD (0x" + to_hex(addr) + "), A | Val: 0x" + to_hex(a, 2);
                break;
            }

            // === Group 3: 16-bit Load Operations ===
            case 0x31: // LD SP, nn (Set Stack Pointer)
                sp = fetch16(mem);
                msg = "[0x" + to_hex(current_pc) + "] LD SP, 0x" + to_hex(sp);
                break;

            // === Group 4: Arithmetic Operations ===
            case 0x3C: // INC A (Increment Accumulator)
                a++;
                msg = "[0x" + to_hex(current_pc) + "] INC A | A=0x" + to_hex(a, 2);
                break;

            // === Group 5: Control Flow (Jumps & Subroutines) ===
            case 0xCD: { // CALL nn (Call Subroutine)
                uint16_t target = fetch16(mem);
                msg = "[0x" + to_hex(current_pc) + "] CALL 0x" + to_hex(target) + " | Pushing Return PC: 0x" + to_hex(pc);
                push16(mem, pc);
                pc = target;
                break;
            }

            case 0xC3: { // JP nn (Unconditional Jump)
                uint16_t target = fetch16(mem);
                pc = target;
                msg = "[0x" + to_hex(current_pc) + "] JP 0x" + to_hex(pc);
                break;
            }

            case 0xC9: // RET (Return from Subroutine)
                pc = pop16(mem);
                msg = "[0x" + to_hex(current_pc) + "] RET | Returning to 0x" + to_hex(pc);
                break;

            // === Default: Error Handling ===
            default:
                msg = "[0x" + to_hex(current_pc) + "] UNKNOWN OPCODE: 0x" + to_hex(opcode, 2);
                halted = true;
                break;
        }

        log(trace, msg);
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: ./emulator <programs/filename.bin>" << std::endl;
        return 1;
    }

    std::string binPath = argv[1];
    Memory mem;
    Z80CPU cpu;

    // Attempt to load the binary into memory
    if (!mem.loadFromFile(binPath)) {
        std::cerr << "Error: Could not load " << binPath << std::endl;
        return 1;
    }

    // Auto-generate trace filename by swapping .bin for .trace
    std::string tracePath = binPath;
    size_t lastDot = tracePath.find_last_of(".");
    if (lastDot != std::string::npos) {
        tracePath = tracePath.substr(0, lastDot);
    }
    tracePath += ".trace";

    std::ofstream traceFile(tracePath);
    if (!traceFile.is_open()) {
        std::cerr << "Error: Could not create trace file " << tracePath << std::endl;
        return 1;
    }

    // Header for the trace log
    std::string header = "--- Schneider 6128 Z80 Trace: " + binPath + " ---";
    cpu.log(traceFile, header);

    // Primary Execution Loop with Safety Valve
    int cycles = 0;
    const int MAX_CYCLES = 100; // Stop after 100 steps no matter what

    while (!cpu.halted && cycles < MAX_CYCLES) {
        cpu.step(mem, traceFile);
        cycles++;
    }

    if (cycles >= MAX_CYCLES) {
    cpu.log(traceFile, "--- Safety Limit Reached (" + std::to_string(MAX_CYCLES) + " cycles) ---");
}
    cpu.log(traceFile, "--- Execution Finished ---");
    return 0;
}