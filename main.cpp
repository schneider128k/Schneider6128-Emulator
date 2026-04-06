#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <string>

// --- 1. THE MEMORY SYSTEM ---
struct Memory {
    uint8_t ram[65536];
    void clear() { for (int i = 0; i < 65536; i++) ram[i] = 0; }
    uint8_t read(uint16_t address) { return ram[address]; }
    void write(uint16_t address, uint8_t value) { ram[address] = value; }

    // New: Load a binary file into RAM at a specific offset
    bool loadFromFile(const std::string& filename, uint16_t offset) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return false;

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        if (offset + size > 65536) return false; // Out of bounds check

        if (file.read(reinterpret_cast<char*>(&ram[offset]), size)) {
            std::cout << "[Memory] Loaded " << size << " bytes from " << filename << std::endl;
            return true;
        }
        return false;
    }
};

// --- 2. THE Z80 CPU ---
struct Z80CPU {
    uint8_t a, b; 
    uint8_t f;    
    uint16_t pc; 
    Memory* bus; 

    const uint8_t FLAG_S = 0x80;
    const uint8_t FLAG_Z = 0x40;

    void reset() {
        a = 0; b = 0; f = 0; pc = 0x0000;
        std::cout << "[CPU] Reset. PC at 0x0000" << std::endl;
    }

    void setFlag(uint8_t flagMask, bool condition) {
        if (condition) f |= flagMask;
        else f &= ~flagMask;
    }

    uint8_t fetch() {
        uint8_t data = bus->read(pc);
        pc++;
        return data;
    }

    uint16_t fetchWord() {
        uint8_t low = fetch();
        uint8_t high = fetch();
        return (static_cast<uint16_t>(high) << 8) | low;
    }

    void step() {
        uint8_t opcode = fetch();
        switch (opcode) {
            case 0x00: break; 
            case 0x3E: a = fetch(); std::cout << "[CPU] LD A, " << (int)a << std::endl; break;
            case 0x06: b = fetch(); std::cout << "[CPU] LD B, " << (int)b << std::endl; break;
            case 0x78: a = b; std::cout << "[CPU] LD A, B" << std::endl; break;
            case 0x32: {
                uint16_t addr = fetchWord();
                bus->write(addr, a);
                std::printf("[CPU] LD (0x%04X), A | Stored %d\n", addr, a);
                break;
            }
            case 0x3C: a++; setFlag(FLAG_Z, (a == 0)); setFlag(FLAG_S, (a & 0x80)); break;
            case 0xC3: pc = fetchWord(); std::printf("[CPU] JP 0x%04X\n", pc); break;
            default:
                std::printf("[CPU] Unknown Opcode 0x%02X at 0x%04X\n", opcode, pc - 1);
                break;
        }
    }
};

// --- 3. THE MAIN PROGRAM ---
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: ./emulator <program.bin>" << std::endl;
        return 1;
    }

    Memory myRam; myRam.clear();
    Z80CPU myCpu; myCpu.bus = &myRam; myCpu.reset();

    // Load the file passed via command line
    if (!myRam.loadFromFile(argv[1], 0x0000)) {
        std::cerr << "Error: Could not load file " << argv[1] << std::endl;
        return 1;
    }

    std::cout << "--- Executing " << argv[1] << " ---" << std::endl;
    // Run for 10 steps or until an error
    for (int i = 0; i < 10; i++) {
        myCpu.step();
    }

    return 0;
}