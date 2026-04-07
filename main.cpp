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
 * Milestone 7: Full Register Set + LDIR + All Three Video Modes
 * ----------------------------------------------------------------------------
 * Flag Register (F) bit layout:
 *   Bit 7: S (Sign)        Bit 6: Z (Zero)    Bit 4: H (Half-Carry)
 *   Bit 2: P/V (Parity)    Bit 1: N (Add/Sub)  Bit 0: C (Carry)
 * ----------------------------------------------------------------------------
 * Milestone 7 Additions:
 *   Registers: C, D, E, H, L (completing BC, DE, HL pairs)
 *   LD BC/DE/HL, nn   — 16-bit immediate loads
 *   LD (HL), A        — store A at address pointed to by HL
 *   LD (HL), n        — store immediate at address in HL
 *   LD A, (HL)        — load A from address in HL
 *   INC BC/DE/HL      — 16-bit increment (no flags affected)
 *   DEC BC/DE/HL      — 16-bit decrement (no flags affected)
 *   0xED prefix       — extended opcode family
 *   LDIR (ED B0)      — block copy: (HL)->(DE), INC HL/DE, DEC BC, repeat
 *   LDDR (ED B8)      — block copy backwards
 * ----------------------------------------------------------------------------
 */

const uint8_t FLAG_S = 0x80;
const uint8_t FLAG_Z = 0x40;
const uint8_t FLAG_H = 0x10;
const uint8_t FLAG_P = 0x04;
const uint8_t FLAG_N = 0x02;
const uint8_t FLAG_C = 0x01;

struct Memory {
    std::vector<uint8_t> ram;
    bool vram_dirty = false;
    int  vram_frame = 0;

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
    // 8-bit registers
    uint8_t  a = 0;   // Accumulator
    uint8_t  f = 0;   // Flags
    uint8_t  b = 0, c = 0;   // BC pair
    uint8_t  d = 0, e = 0;   // DE pair
    uint8_t  h = 0, l = 0;   // HL pair (primary pointer register)

    // 16-bit special registers
    uint16_t pc = 0;
    uint16_t sp = 0;

    bool halted = false;

    // --- 16-bit pair accessors ---
    uint16_t BC() const { return (uint16_t)((b << 8) | c); }
    uint16_t DE() const { return (uint16_t)((d << 8) | e); }
    uint16_t HL() const { return (uint16_t)((h << 8) | l); }

    void setBC(uint16_t v) { b = (v >> 8) & 0xFF; c = v & 0xFF; }
    void setDE(uint16_t v) { d = (v >> 8) & 0xFF; e = v & 0xFF; }
    void setHL(uint16_t v) { h = (v >> 8) & 0xFF; l = v & 0xFF; }

    // --- Utilities ---
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

    // --- Memory interaction ---
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

    // --- Conditional jump helpers ---
    void do_jr(Memory& mem, bool cond, std::string& mn, const std::string& tag) {
        int8_t offset = static_cast<int8_t>(fetch(mem));
        if (cond) {
            pc = static_cast<uint16_t>(static_cast<int32_t>(pc) + offset);
            mn = "JR " + tag + " -> 0x" + to_hex(pc);
        } else {
            mn = "JR " + tag + " [not taken]";
        }
    }

    void do_jp_cond(Memory& mem, bool cond, std::string& mn, const std::string& tag) {
        uint16_t target = fetch16(mem);
        if (cond) { pc = target; mn = "JP " + tag + ", 0x" + to_hex(pc) + " [taken]"; }
        else       {             mn = "JP " + tag + ", 0x" + to_hex(target) + " [not taken]"; }
    }

    // --- ED prefix handler ---
    void step_ED(Memory& mem, std::ostream& trace, uint16_t current_pc) {
        uint8_t op2 = fetch(mem);
        std::string mnemonic;

        switch (op2) {

        case 0xB0: { // LDIR — copy BC bytes from (HL) to (DE), increment both, repeat
            // Each iteration: write one byte, advance pointers, decrement counter
            // We execute the full block in one step() call for simplicity.
            // The trace shows a single LDIR entry with the final register state.
            uint16_t count = BC();
            while (count > 0) {
                mem.write(DE(), mem.read(HL()));
                setHL(HL() + 1);
                setDE(DE() + 1);
                count--;
            }
            setBC(0);
            setFlag(FLAG_Z, true);   // BC reached 0
            setFlag(FLAG_P, false);  // P/V = 0 when BC=0
            mnemonic = "LDIR";
            break;
        }

        case 0xB8: { // LDDR — copy BC bytes from (HL) to (DE), decrement both, repeat
            uint16_t count = BC();
            while (count > 0) {
                mem.write(DE(), mem.read(HL()));
                setHL(HL() - 1);
                setDE(DE() - 1);
                count--;
            }
            setBC(0);
            setFlag(FLAG_Z, true);
            setFlag(FLAG_P, false);
            mnemonic = "LDDR";
            break;
        }

        default:
            mnemonic = "UNKNOWN ED: 0x" + to_hex(op2, 2);
            halted = true;
            break;
        }

        // Trace
        std::stringstream ss;
        ss << "[" << to_hex(current_pc) << "] "
           << std::left << std::setw(30) << mnemonic
           << " | A:" << to_hex(a,2)
           << " BC:" << to_hex(BC(),4)
           << " DE:" << to_hex(DE(),4)
           << " HL:" << to_hex(HL(),4)
           << " F:" << std::bitset<8>(f)
           << " [Z:" << (flagZ()?'1':'0') << " C:" << (flagC()?'1':'0') << "]";
        log(trace, ss.str());
    }

    // --- Main instruction cycle ---
    void step(Memory& mem, std::ostream& trace) {
        if (halted) return;

        uint16_t current_pc = pc;
        uint8_t  opcode     = fetch(mem);
        std::string mnemonic;

        switch (opcode) {

        // === System Control ===
        case 0x00: mnemonic = "NOP";  break;
        case 0x76: halted = true; mnemonic = "HALT"; break;

        // === 8-bit Loads — immediate ===
        case 0x3E: a = fetch(mem); mnemonic = "LD A, 0x"  + to_hex(a,2); break;
        case 0x06: b = fetch(mem); mnemonic = "LD B, 0x"  + to_hex(b,2); break;
        case 0x0E: c = fetch(mem); mnemonic = "LD C, 0x"  + to_hex(c,2); break;
        case 0x16: d = fetch(mem); mnemonic = "LD D, 0x"  + to_hex(d,2); break;
        case 0x1E: e = fetch(mem); mnemonic = "LD E, 0x"  + to_hex(e,2); break;
        case 0x26: h = fetch(mem); mnemonic = "LD H, 0x"  + to_hex(h,2); break;
        case 0x2E: l = fetch(mem); mnemonic = "LD L, 0x"  + to_hex(l,2); break;

        // === 8-bit Loads — register to register ===
        case 0x78: a = b; mnemonic = "LD A, B"; break;
        case 0x79: a = c; mnemonic = "LD A, C"; break;
        case 0x7A: a = d; mnemonic = "LD A, D"; break;
        case 0x7B: a = e; mnemonic = "LD A, E"; break;
        case 0x7C: a = h; mnemonic = "LD A, H"; break;
        case 0x7D: a = l; mnemonic = "LD A, L"; break;

        // === 8-bit Loads — HL indirect ===
        case 0x77: // LD (HL), A
            mem.write(HL(), a);
            mnemonic = "LD (HL), A";
            break;
        case 0x36: { // LD (HL), n
            uint8_t n = fetch(mem);
            mem.write(HL(), n);
            mnemonic = "LD (HL), 0x" + to_hex(n,2);
            break;
        }
        case 0x7E: // LD A, (HL)
            a = mem.read(HL());
            mnemonic = "LD A, (HL)";
            break;

        // === 8-bit Loads — absolute address ===
        case 0x32: { // LD (nn), A
            uint16_t addr = fetch16(mem);
            mem.write(addr, a);
            mnemonic = "LD (0x" + to_hex(addr) + "), A";
            break;
        }
        case 0x3A: { // LD A, (nn)
            uint16_t addr = fetch16(mem);
            a = mem.read(addr);
            mnemonic = "LD A, (0x" + to_hex(addr) + ")";
            break;
        }

        // === 16-bit Loads ===
        case 0x01: setBC(fetch16(mem)); mnemonic = "LD BC, 0x" + to_hex(BC()); break;
        case 0x11: setDE(fetch16(mem)); mnemonic = "LD DE, 0x" + to_hex(DE()); break;
        case 0x21: setHL(fetch16(mem)); mnemonic = "LD HL, 0x" + to_hex(HL()); break;
        case 0x31: sp = fetch16(mem);  mnemonic = "LD SP, 0x" + to_hex(sp);   break;

        // === 16-bit Increments / Decrements (no flags) ===
        case 0x03: setBC(BC()+1); mnemonic = "INC BC"; break;
        case 0x13: setDE(DE()+1); mnemonic = "INC DE"; break;
        case 0x23: setHL(HL()+1); mnemonic = "INC HL"; break;
        case 0x0B: setBC(BC()-1); mnemonic = "DEC BC"; break;
        case 0x1B: setDE(DE()-1); mnemonic = "DEC DE"; break;
        case 0x2B: setHL(HL()-1); mnemonic = "DEC HL"; break;

        // === 8-bit Increments / Decrements ===
        case 0x3C: a++; setFlag(FLAG_Z,a==0); setFlag(FLAG_S,(a&0x80)!=0); mnemonic="INC A"; break;
        case 0x04: b++; setFlag(FLAG_Z,b==0); setFlag(FLAG_S,(b&0x80)!=0); mnemonic="INC B"; break;
        case 0x0C: c++; setFlag(FLAG_Z,c==0); setFlag(FLAG_S,(c&0x80)!=0); mnemonic="INC C"; break;
        case 0x05: b--; setFlag(FLAG_Z,b==0); setFlag(FLAG_S,(b&0x80)!=0); mnemonic="DEC B"; break;
        case 0x0D: c--; setFlag(FLAG_Z,c==0); setFlag(FLAG_S,(c&0x80)!=0); mnemonic="DEC C"; break;

        // === Arithmetic & Logic ===
        case 0xFE: { // CP n
            uint8_t n = fetch(mem);
            uint8_t r = a - n;
            setFlag(FLAG_Z, r==0); setFlag(FLAG_C, a<n);
            mnemonic = "CP 0x" + to_hex(n,2);
            break;
        }
        case 0xB8: { // CP B
            uint8_t r = a - b;
            setFlag(FLAG_Z, r==0); setFlag(FLAG_C, a<b);
            mnemonic = "CP B";
            break;
        }

        // === Unconditional Jumps ===
        case 0xC3: pc = fetch16(mem); mnemonic = "JP 0x" + to_hex(pc); break;
        case 0x18: {
            int8_t o = static_cast<int8_t>(fetch(mem));
            pc = static_cast<uint16_t>(static_cast<int32_t>(pc) + o);
            mnemonic = "JR 0x" + to_hex(pc);
            break;
        }

        // === Conditional Absolute Jumps ===
        case 0xC2: do_jp_cond(mem, !flagZ(), mnemonic, "NZ"); break;
        case 0xCA: do_jp_cond(mem,  flagZ(), mnemonic, "Z");  break;
        case 0xD2: do_jp_cond(mem, !flagC(), mnemonic, "NC"); break;
        case 0xDA: do_jp_cond(mem,  flagC(), mnemonic, "C");  break;

        // === Conditional Relative Jumps ===
        case 0x20: do_jr(mem, !flagZ(), mnemonic, "NZ"); break;
        case 0x28: do_jr(mem,  flagZ(), mnemonic, "Z");  break;
        case 0x30: do_jr(mem, !flagC(), mnemonic, "NC"); break;
        case 0x38: do_jr(mem,  flagC(), mnemonic, "C");  break;

        // === Stack & Subroutines ===
        case 0xCD: { uint16_t t = fetch16(mem); mnemonic="CALL 0x"+to_hex(t); push16(mem,pc); pc=t; break; }
        case 0xC9: pc = pop16(mem); mnemonic = "RET"; break;

        // === Extended Opcode Prefix ===
        case 0xED:
            step_ED(mem, trace, current_pc);
            return;   // step_ED handles its own trace line

        default:
            mnemonic = "UNKNOWN: 0x" + to_hex(opcode,2);
            halted = true;
            break;
        }

        // Trace
        std::stringstream ss;
        ss << "[" << to_hex(current_pc) << "] "
           << std::left << std::setw(30) << mnemonic
           << " | A:" << to_hex(a,2)
           << " BC:" << to_hex(BC(),4)
           << " DE:" << to_hex(DE(),4)
           << " HL:" << to_hex(HL(),4)
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

    // Trace path
    std::string tracePath = binPath;
    size_t lastDot = tracePath.find_last_of(".");
    if (lastDot != std::string::npos) tracePath = tracePath.substr(0, lastDot);
    tracePath += ".trace";

    std::ofstream traceFile(tracePath);
    if (!traceFile.is_open()) return 1;

    cpu.log(traceFile, "--- WPS-Z80 NEURAL TRACE: " + binPath + " ---");

    int cycles = 0;
    const int MAX_CYCLES = 5000;

    while (!cpu.halted && cycles < MAX_CYCLES) {
        cpu.step(mem, traceFile);
        cycles++;
    }

    if (cycles >= MAX_CYCLES) cpu.log(traceFile, "--- Safety Limit Reached ---");

    // VRAM dump
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