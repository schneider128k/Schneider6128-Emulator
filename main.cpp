#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <bitset>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

/**
 * WOCJAN PERCUSSIVE SYSTEMS - Z80 CORE UNIT
 * Milestone 6: Monitor Output (Mode 1 VRAM Dump)
 * ----------------------------------------------------------------------------
 * Flag Register (F) bit layout:
 *   Bit 7: S (Sign)        Bit 6: Z (Zero)    Bit 4: H (Half-Carry)
 *   Bit 2: P/V (Parity)    Bit 1: N (Add/Sub)  Bit 0: C (Carry)
 * ----------------------------------------------------------------------------
 * Milestone 6 Additions:
 *   - Memory::vram_dirty flag: set on any write to 0xC000-0xFFFF
 *   - Memory::dumpVRAM(): writes raw 16KB block to <stem>_vram/frame_NNNN.vram
 *   - VRAM folder created automatically alongside the .bin file
 *   - monitor.py reads the folder and renders Mode 1 live in a pygame window
 * ----------------------------------------------------------------------------
 */

// WPS-Standard Flag Masks
const uint8_t FLAG_S = 0x80;
const uint8_t FLAG_Z = 0x40;
const uint8_t FLAG_H = 0x10;
const uint8_t FLAG_P = 0x04;
const uint8_t FLAG_N = 0x02;
const uint8_t FLAG_C = 0x01;

struct Memory {
    std::vector<uint8_t> ram;
    bool vram_dirty = false;   // true when 0xC000-0xFFFF has been written
    int  vram_frame = 0;       // sequential frame counter for dump filenames

    Memory() : ram(65536, 0) {}

    uint8_t read(uint16_t address) const { return ram[address]; }

    void write(uint16_t address, uint8_t value) {
        ram[address] = value;
        if (address >= 0xC000) vram_dirty = true;
    }

    bool loadFromFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) return false;
        file.read(reinterpret_cast<char*>(ram.data()), ram.size());
        return true;
    }

    // Dump the 16KB VRAM block (0xC000-0xFFFF) to vram_dir/frame_NNNN.vram.
    // Called after execution completes. Future milestones may call this
    // periodically (e.g. every N cycles) to produce animation frames.
    void dumpVRAM(const std::string& vram_dir) {
        if (!vram_dirty) return;
        std::ostringstream fname;
        fname << vram_dir << "/frame_"
              << std::setw(4) << std::setfill('0') << (++vram_frame)
              << ".vram";
        std::ofstream f(fname.str(), std::ios::binary);
        if (!f.is_open()) {
            std::cerr << "Warning: could not write " << fname.str() << std::endl;
            return;
        }
        f.write(reinterpret_cast<const char*>(&ram[0xC000]), 0x4000);
        vram_dirty = false;
    }
};

struct Z80CPU {
    uint8_t  a = 0, b = 0, f = 0;
    uint16_t pc = 0, sp = 0;
    bool halted = false;

    std::string to_hex(uint16_t val, int width = 4) {
        std::stringstream ss;
        ss << std::uppercase << std::hex << std::setw(width) << std::setfill('0') << val;
        return ss.str();
    }

    void log(std::ostream& os, const std::string& message) {
        std::cout << message << std::endl;
        os << message << "\n";
    }

    void setFlag(uint8_t mask, bool condition) {
        if (condition) f |= mask; else f &= ~mask;
    }

    bool flagZ() const { return (f & FLAG_Z) != 0; }
    bool flagC() const { return (f & FLAG_C) != 0; }

    uint8_t fetch(Memory& mem)   { return mem.read(pc++); }

    uint16_t fetch16(Memory& mem) {
        uint16_t lo = fetch(mem), hi = fetch(mem);
        return (hi << 8) | lo;
    }

    void push16(Memory& mem, uint16_t value) {
        sp--; mem.write(sp, (value >> 8) & 0xFF);
        sp--; mem.write(sp,  value       & 0xFF);
    }

    uint16_t pop16(Memory& mem) {
        uint8_t lo = mem.read(sp++), hi = mem.read(sp++);
        return (hi << 8) | lo;
    }

    void do_jr(Memory& mem, bool condition, std::string& mnemonic, const std::string& tag) {
        int8_t offset = static_cast<int8_t>(fetch(mem));
        if (condition) {
            pc = static_cast<uint16_t>(static_cast<int32_t>(pc) + offset);
            mnemonic = "JR " + tag + " -> 0x" + to_hex(pc);
        } else {
            mnemonic = "JR " + tag + " [not taken]";
        }
    }

    void do_jp_cond(Memory& mem, bool condition, std::string& mnemonic, const std::string& tag) {
        uint16_t target = fetch16(mem);
        if (condition) {
            pc = target;
            mnemonic = "JP " + tag + ", 0x" + to_hex(pc) + " [taken]";
        } else {
            mnemonic = "JP " + tag + ", 0x" + to_hex(target) + " [not taken]";
        }
    }

    void step(Memory& mem, std::ostream& trace) {
        if (halted) return;
        uint16_t current_pc = pc;
        uint8_t  opcode     = fetch(mem);
        std::string mnemonic;

        switch (opcode) {
        case 0x00: mnemonic = "NOP"; break;
        case 0x76: halted = true; mnemonic = "HALT"; break;

        case 0x3E: a = fetch(mem); mnemonic = "LD A, 0x" + to_hex(a, 2); break;
        case 0x06: b = fetch(mem); mnemonic = "LD B, 0x" + to_hex(b, 2); break;
        case 0x78: a = b;          mnemonic = "LD A, B";                  break;
        case 0x32: { uint16_t addr = fetch16(mem); mem.write(addr, a);
                     mnemonic = "LD (0x" + to_hex(addr) + "), A"; break; }

        case 0x31: sp = fetch16(mem); mnemonic = "LD SP, 0x" + to_hex(sp); break;

        case 0x3C: a++; setFlag(FLAG_Z, a==0); setFlag(FLAG_S,(a&0x80)!=0);
                   mnemonic = "INC A"; break;
        case 0x05: b--; setFlag(FLAG_Z, b==0); setFlag(FLAG_S,(b&0x80)!=0);
                   mnemonic = "DEC B"; break;
        case 0xFE: { uint8_t n = fetch(mem); uint8_t r = a - n;
                     setFlag(FLAG_Z, r==0); setFlag(FLAG_C, a<n);
                     mnemonic = "CP 0x" + to_hex(n, 2); break; }

        case 0xC3: pc = fetch16(mem); mnemonic = "JP 0x" + to_hex(pc); break;
        case 0xC2: do_jp_cond(mem, !flagZ(), mnemonic, "NZ"); break;
        case 0xCA: do_jp_cond(mem,  flagZ(), mnemonic, "Z");  break;
        case 0xD2: do_jp_cond(mem, !flagC(), mnemonic, "NC"); break;
        case 0xDA: do_jp_cond(mem,  flagC(), mnemonic, "C");  break;

        case 0x18: { int8_t o = static_cast<int8_t>(fetch(mem));
                     pc = static_cast<uint16_t>(static_cast<int32_t>(pc) + o);
                     mnemonic = "JR 0x" + to_hex(pc); break; }
        case 0x20: do_jr(mem, !flagZ(), mnemonic, "NZ"); break;
        case 0x28: do_jr(mem,  flagZ(), mnemonic, "Z");  break;
        case 0x30: do_jr(mem, !flagC(), mnemonic, "NC"); break;
        case 0x38: do_jr(mem,  flagC(), mnemonic, "C");  break;

        case 0xCD: { uint16_t t = fetch16(mem); mnemonic = "CALL 0x" + to_hex(t);
                     push16(mem, pc); pc = t; break; }
        case 0xC9: pc = pop16(mem); mnemonic = "RET"; break;

        default: mnemonic = "UNKNOWN: 0x" + to_hex(opcode, 2); halted = true; break;
        }

        std::stringstream ss;
        ss << "[" << to_hex(current_pc) << "] "
           << std::left << std::setw(30) << mnemonic
           << " | A:" << to_hex(a,2) << " B:" << to_hex(b,2)
           << " F:" << std::bitset<8>(f)
           << " [Z:" << (flagZ()?'1':'0') << " C:" << (flagC()?'1':'0') << "]";
        log(trace, ss.str());
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: ./emulator <programs/filename.bin>" << std::endl;
        return 1;
    }

    std::string binPath = argv[1];
    Memory mem; Z80CPU cpu;

    if (!mem.loadFromFile(binPath)) {
        std::cerr << "Error: Could not load " << binPath << std::endl;
        return 1;
    }

    // Derive trace path: programs/lesson6.bin -> programs/lesson6.trace
    std::string tracePath = binPath;
    size_t lastDot = tracePath.find_last_of(".");
    if (lastDot != std::string::npos) tracePath = tracePath.substr(0, lastDot);
    tracePath += ".trace";

    std::ofstream traceFile(tracePath);
    if (!traceFile.is_open()) return 1;

    cpu.log(traceFile, "--- WPS-Z80 NEURAL TRACE: " + binPath + " ---");

    int cycles = 0;
    const int MAX_CYCLES = 5000;   // raised for lesson6 (fills 16KB VRAM)

    while (!cpu.halted && cycles < MAX_CYCLES) {
        cpu.step(mem, traceFile);
        cycles++;
    }

    if (cycles >= MAX_CYCLES) cpu.log(traceFile, "--- Safety Limit Reached ---");

    // Derive VRAM folder: programs/lesson6.bin -> programs/lesson6_vram/
    std::string vramDir = binPath;
    size_t dot = vramDir.find_last_of(".");
    if (dot != std::string::npos) vramDir = vramDir.substr(0, dot);
    vramDir += "_vram";

    #ifdef _WIN32
        _mkdir(vramDir.c_str());
    #else
        mkdir(vramDir.c_str(), 0755);
    #endif

    mem.dumpVRAM(vramDir);
    if (mem.vram_frame > 0)
        cpu.log(traceFile, "--- VRAM dumped: " + vramDir + " ---");

    cpu.log(traceFile, "--- Execution Finished ---");
    return 0;
}