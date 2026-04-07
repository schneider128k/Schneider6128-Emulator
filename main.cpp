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
 * Milestone 8: Full ALU (ADD, SUB, AND, OR, XOR, ADD HL rr)
 * ----------------------------------------------------------------------------
 * Flag Register (F) bit layout:
 *   Bit 7: S (Sign)        Bit 6: Z (Zero)    Bit 4: H (Half-Carry)
 *   Bit 2: P/V (Parity)    Bit 1: N (Add/Sub)  Bit 0: C (Carry)
 * ----------------------------------------------------------------------------
 * Milestone 8 Additions:
 *   ADD A, r/n    — 8-bit add; sets S, Z, H, C; clears N
 *   SUB r/n       — 8-bit subtract; sets S, Z, H, C; sets N
 *   AND r/n       — bitwise AND; sets S, Z, P; sets H; clears N, C
 *   OR  r/n       — bitwise OR;  sets S, Z, P; clears H, N, C
 *   XOR r/n       — bitwise XOR; sets S, Z, P; clears H, N, C
 *   ADD HL, rr    — 16-bit add; updates C only; clears N
 *   LD D/E, (HL)  — indirect loads into D and E
 *   LD (DE), A    — store A at address in DE
 *   LD A, (DE)    — load A from address in DE
 * ----------------------------------------------------------------------------
 */

// WPS-Standard Flag Masks
const uint8_t FLAG_S = 0x80;
const uint8_t FLAG_Z = 0x40;
const uint8_t FLAG_H = 0x10;
const uint8_t FLAG_P = 0x04;
const uint8_t FLAG_N = 0x02;
const uint8_t FLAG_C = 0x01;

// Parity: returns true if number of set bits is even
static bool parity(uint8_t v) {
    v ^= v >> 4; v ^= v >> 2; v ^= v >> 1;
    return (v & 1) == 0;
}

// ---------------------------------------------------------------------------
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

    // Dump the 16KB VRAM block to <vram_dir>/frame_NNNN.vram
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

// ---------------------------------------------------------------------------
struct Z80CPU {
    // 8-bit registers
    uint8_t  a = 0, f = 0;
    uint8_t  b = 0, c = 0;
    uint8_t  d = 0, e = 0;
    uint8_t  h = 0, l = 0;

    // 16-bit special registers
    uint16_t pc = 0, sp = 0;

    bool halted = false;

    // --- 16-bit pair accessors (computed from 8-bit halves) ---
    uint16_t BC() const { return (uint16_t)((b << 8) | c); }
    uint16_t DE() const { return (uint16_t)((d << 8) | e); }
    uint16_t HL() const { return (uint16_t)((h << 8) | l); }
    void setBC(uint16_t v) { b = (v >> 8) & 0xFF; c = v & 0xFF; }
    void setDE(uint16_t v) { d = (v >> 8) & 0xFF; e = v & 0xFF; }
    void setHL(uint16_t v) { h = (v >> 8) & 0xFF; l = v & 0xFF; }

    // --- Utilities ---
    std::string to_hex(uint16_t val, int width = 4) {
        std::stringstream ss;
        ss << std::uppercase << std::hex
           << std::setw(width) << std::setfill('0') << val;
        return ss.str();
    }

    void log(std::ostream& os, const std::string& msg) {
        std::cout << msg << std::endl;
        os << msg << "\n";
    }

    void setFlag(uint8_t mask, bool cond) {
        if (cond) f |= mask; else f &= ~mask;
    }

    bool flagZ() const { return (f & FLAG_Z) != 0; }
    bool flagC() const { return (f & FLAG_C) != 0; }

    // --- Memory interaction ---
    uint8_t fetch(Memory& mem) { return mem.read(pc++); }

    uint16_t fetch16(Memory& mem) {
        uint16_t lo = fetch(mem), hi = fetch(mem);
        return (hi << 8) | lo;
    }

    void push16(Memory& mem, uint16_t v) {
        sp--; mem.write(sp, (v >> 8) & 0xFF);
        sp--; mem.write(sp,  v       & 0xFF);
    }

    uint16_t pop16(Memory& mem) {
        uint8_t lo = mem.read(sp++), hi = mem.read(sp++);
        return (hi << 8) | lo;
    }

    // --- ALU flag helpers ---

    // Flags for 8-bit ADD: S, Z, H, C, clears N
    void flags_add(uint8_t operand, uint16_t result) {
        setFlag(FLAG_S, (result & 0x80) != 0);
        setFlag(FLAG_Z, (result & 0xFF) == 0);
        setFlag(FLAG_H, ((a & 0x0F) + (operand & 0x0F)) > 0x0F);
        setFlag(FLAG_C,  result > 0xFF);
        setFlag(FLAG_N,  false);
    }

    // Flags for 8-bit SUB/CP: S, Z, H, C, sets N
    // NOTE: call before modifying A
    void flags_sub(uint8_t operand, uint16_t result) {
        setFlag(FLAG_S, (result & 0x80) != 0);
        setFlag(FLAG_Z, (result & 0xFF) == 0);
        setFlag(FLAG_H, (a & 0x0F) < (operand & 0x0F));
        setFlag(FLAG_C,  a < operand);
        setFlag(FLAG_N,  true);
    }

    // Flags for AND: S, Z, P; H=1; N=0, C=0
    void flags_and(uint8_t result) {
        setFlag(FLAG_S, (result & 0x80) != 0);
        setFlag(FLAG_Z,  result == 0);
        setFlag(FLAG_H,  true);
        setFlag(FLAG_P,  parity(result));
        setFlag(FLAG_N,  false);
        setFlag(FLAG_C,  false);
    }

    // Flags for OR/XOR: S, Z, P; H=0; N=0, C=0
    void flags_or_xor(uint8_t result) {
        setFlag(FLAG_S, (result & 0x80) != 0);
        setFlag(FLAG_Z,  result == 0);
        setFlag(FLAG_H,  false);
        setFlag(FLAG_P,  parity(result));
        setFlag(FLAG_N,  false);
        setFlag(FLAG_C,  false);
    }

    // --- Conditional jump helpers ---
    void do_jr(Memory& mem, bool cond,
               std::string& mn, const std::string& tag) {
        int8_t o = static_cast<int8_t>(fetch(mem));
        if (cond) {
            pc = static_cast<uint16_t>(static_cast<int32_t>(pc) + o);
            mn = "JR " + tag + " -> 0x" + to_hex(pc);
        } else {
            mn = "JR " + tag + " [not taken]";
        }
    }

    void do_jp_cond(Memory& mem, bool cond,
                    std::string& mn, const std::string& tag) {
        uint16_t t = fetch16(mem);
        if (cond) { pc = t; mn = "JP " + tag + ", 0x" + to_hex(pc) + " [taken]"; }
        else       {         mn = "JP " + tag + ", 0x" + to_hex(t)  + " [not taken]"; }
    }

    // --- ED prefix handler ---
    void step_ED(Memory& mem, std::ostream& trace, uint16_t current_pc) {
        uint8_t op2 = fetch(mem);
        std::string mnemonic;

        switch (op2) {
        case 0xB0: { // LDIR
            uint16_t count = BC();
            while (count > 0) {
                mem.write(DE(), mem.read(HL()));
                setHL(HL() + 1); setDE(DE() + 1); count--;
            }
            setBC(0);
            setFlag(FLAG_Z, true);
            setFlag(FLAG_P, false);
            mnemonic = "LDIR";
            break;
        }
        case 0xB8: { // LDDR
            uint16_t count = BC();
            while (count > 0) {
                mem.write(DE(), mem.read(HL()));
                setHL(HL() - 1); setDE(DE() - 1); count--;
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

        std::stringstream ss;
        ss << "[" << to_hex(current_pc) << "] "
           << std::left << std::setw(30) << mnemonic
           << " | A:" << to_hex(a, 2)
           << " BC:" << to_hex(BC(), 4)
           << " DE:" << to_hex(DE(), 4)
           << " HL:" << to_hex(HL(), 4)
           << " F:" << std::bitset<8>(f)
           << " [Z:" << (flagZ() ? '1' : '0')
           << " C:" << (flagC() ? '1' : '0') << "]";
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
        case 0x3E: a = fetch(mem); mnemonic = "LD A, 0x" + to_hex(a, 2); break;
        case 0x06: b = fetch(mem); mnemonic = "LD B, 0x" + to_hex(b, 2); break;
        case 0x0E: c = fetch(mem); mnemonic = "LD C, 0x" + to_hex(c, 2); break;
        case 0x16: d = fetch(mem); mnemonic = "LD D, 0x" + to_hex(d, 2); break;
        case 0x1E: e = fetch(mem); mnemonic = "LD E, 0x" + to_hex(e, 2); break;
        case 0x26: h = fetch(mem); mnemonic = "LD H, 0x" + to_hex(h, 2); break;
        case 0x2E: l = fetch(mem); mnemonic = "LD L, 0x" + to_hex(l, 2); break;

        // === 8-bit Loads — register to register ===
        case 0x78: a = b; mnemonic = "LD A, B"; break;
        case 0x79: a = c; mnemonic = "LD A, C"; break;
        case 0x7A: a = d; mnemonic = "LD A, D"; break;
        case 0x7B: a = e; mnemonic = "LD A, E"; break;
        case 0x7C: a = h; mnemonic = "LD A, H"; break;
        case 0x7D: a = l; mnemonic = "LD A, L"; break;
        case 0x47: b = a; mnemonic = "LD B, A"; break;
        case 0x4F: c = a; mnemonic = "LD C, A"; break;
        case 0x57: d = a; mnemonic = "LD D, A"; break;
        case 0x5F: e = a; mnemonic = "LD E, A"; break;
        case 0x67: h = a; mnemonic = "LD H, A"; break;
        case 0x6F: l = a; mnemonic = "LD L, A"; break;

        // === 8-bit Loads — HL indirect ===
        case 0x77: mem.write(HL(), a);     mnemonic = "LD (HL), A";  break;
        case 0x36: { uint8_t n = fetch(mem); mem.write(HL(), n);
                     mnemonic = "LD (HL), 0x" + to_hex(n, 2); break; }
        case 0x7E: a = mem.read(HL());     mnemonic = "LD A, (HL)";  break;
        case 0x46: b = mem.read(HL());     mnemonic = "LD B, (HL)";  break;
        case 0x4E: c = mem.read(HL());     mnemonic = "LD C, (HL)";  break;
        case 0x56: d = mem.read(HL());     mnemonic = "LD D, (HL)";  break;
        case 0x5E: e = mem.read(HL());     mnemonic = "LD E, (HL)";  break;
        case 0x66: h = mem.read(HL());     mnemonic = "LD H, (HL)";  break;
        case 0x6E: l = mem.read(HL());     mnemonic = "LD L, (HL)";  break;

        // === 8-bit Loads — DE indirect ===
        case 0x12: mem.write(DE(), a);     mnemonic = "LD (DE), A";  break;
        case 0x1A: a = mem.read(DE());     mnemonic = "LD A, (DE)";  break;

        // === 8-bit Loads — absolute address ===
        case 0x32: { uint16_t addr = fetch16(mem); mem.write(addr, a);
                     mnemonic = "LD (0x" + to_hex(addr) + "), A"; break; }
        case 0x3A: { uint16_t addr = fetch16(mem); a = mem.read(addr);
                     mnemonic = "LD A, (0x" + to_hex(addr) + ")"; break; }

        // === 16-bit Loads ===
        case 0x01: setBC(fetch16(mem)); mnemonic = "LD BC, 0x" + to_hex(BC()); break;
        case 0x11: setDE(fetch16(mem)); mnemonic = "LD DE, 0x" + to_hex(DE()); break;
        case 0x21: setHL(fetch16(mem)); mnemonic = "LD HL, 0x" + to_hex(HL()); break;
        case 0x31: sp = fetch16(mem);   mnemonic = "LD SP, 0x" + to_hex(sp);   break;

        // === 16-bit Inc/Dec (no flags affected) ===
        case 0x03: setBC(BC() + 1); mnemonic = "INC BC"; break;
        case 0x13: setDE(DE() + 1); mnemonic = "INC DE"; break;
        case 0x23: setHL(HL() + 1); mnemonic = "INC HL"; break;
        case 0x0B: setBC(BC() - 1); mnemonic = "DEC BC"; break;
        case 0x1B: setDE(DE() - 1); mnemonic = "DEC DE"; break;
        case 0x2B: setHL(HL() - 1); mnemonic = "DEC HL"; break;

        // === 8-bit Inc/Dec ===
        case 0x3C: a++; setFlag(FLAG_Z,a==0); setFlag(FLAG_S,(a&0x80)!=0); mnemonic="INC A"; break;
        case 0x04: b++; setFlag(FLAG_Z,b==0); setFlag(FLAG_S,(b&0x80)!=0); mnemonic="INC B"; break;
        case 0x0C: c++; setFlag(FLAG_Z,c==0); setFlag(FLAG_S,(c&0x80)!=0); mnemonic="INC C"; break;
        case 0x14: d++; setFlag(FLAG_Z,d==0); setFlag(FLAG_S,(d&0x80)!=0); mnemonic="INC D"; break;
        case 0x1C: e++; setFlag(FLAG_Z,e==0); setFlag(FLAG_S,(e&0x80)!=0); mnemonic="INC E"; break;
        case 0x24: h++; setFlag(FLAG_Z,h==0); setFlag(FLAG_S,(h&0x80)!=0); mnemonic="INC H"; break;
        case 0x2C: l++; setFlag(FLAG_Z,l==0); setFlag(FLAG_S,(l&0x80)!=0); mnemonic="INC L"; break;
        case 0x05: b--; setFlag(FLAG_Z,b==0); setFlag(FLAG_S,(b&0x80)!=0); mnemonic="DEC B"; break;
        case 0x0D: c--; setFlag(FLAG_Z,c==0); setFlag(FLAG_S,(c&0x80)!=0); mnemonic="DEC C"; break;
        case 0x15: d--; setFlag(FLAG_Z,d==0); setFlag(FLAG_S,(d&0x80)!=0); mnemonic="DEC D"; break;
        case 0x1D: e--; setFlag(FLAG_Z,e==0); setFlag(FLAG_S,(e&0x80)!=0); mnemonic="DEC E"; break;
        case 0x25: h--; setFlag(FLAG_Z,h==0); setFlag(FLAG_S,(h&0x80)!=0); mnemonic="DEC H"; break;
        case 0x2D: l--; setFlag(FLAG_Z,l==0); setFlag(FLAG_S,(l&0x80)!=0); mnemonic="DEC L"; break;
        case 0x3D: a--; setFlag(FLAG_Z,a==0); setFlag(FLAG_S,(a&0x80)!=0); mnemonic="DEC A"; break;

        // === ADD A, r/n ===
        case 0x87: { uint16_t r=(uint16_t)a+a; flags_add(a,r); a=(uint8_t)r; mnemonic="ADD A, A"; break; }
        case 0x80: { uint16_t r=(uint16_t)a+b; flags_add(b,r); a=(uint8_t)r; mnemonic="ADD A, B"; break; }
        case 0x81: { uint16_t r=(uint16_t)a+c; flags_add(c,r); a=(uint8_t)r; mnemonic="ADD A, C"; break; }
        case 0x82: { uint16_t r=(uint16_t)a+d; flags_add(d,r); a=(uint8_t)r; mnemonic="ADD A, D"; break; }
        case 0x83: { uint16_t r=(uint16_t)a+e; flags_add(e,r); a=(uint8_t)r; mnemonic="ADD A, E"; break; }
        case 0x84: { uint16_t r=(uint16_t)a+h; flags_add(h,r); a=(uint8_t)r; mnemonic="ADD A, H"; break; }
        case 0x85: { uint16_t r=(uint16_t)a+l; flags_add(l,r); a=(uint8_t)r; mnemonic="ADD A, L"; break; }
        case 0xC6: { uint8_t n=fetch(mem); uint16_t r=(uint16_t)a+n;
                     flags_add(n,r); a=(uint8_t)r;
                     mnemonic="ADD A, 0x"+to_hex(n,2); break; }

        // === SUB r/n ===
        case 0x97: { uint8_t old=a; uint16_t r=(uint16_t)a-a;
                     flags_sub(old,r); a=(uint8_t)r; mnemonic="SUB A"; break; }
        case 0x90: { uint16_t r=(uint16_t)a-b; flags_sub(b,r); a=(uint8_t)r; mnemonic="SUB B"; break; }
        case 0x91: { uint16_t r=(uint16_t)a-c; flags_sub(c,r); a=(uint8_t)r; mnemonic="SUB C"; break; }
        case 0x92: { uint16_t r=(uint16_t)a-d; flags_sub(d,r); a=(uint8_t)r; mnemonic="SUB D"; break; }
        case 0x93: { uint16_t r=(uint16_t)a-e; flags_sub(e,r); a=(uint8_t)r; mnemonic="SUB E"; break; }
        case 0xD6: { uint8_t n=fetch(mem); uint16_t r=(uint16_t)a-n;
                     flags_sub(n,r); a=(uint8_t)r;
                     mnemonic="SUB 0x"+to_hex(n,2); break; }

        // === CP r/n (compare — sets flags, A unchanged) ===
        case 0xFE: { uint8_t n=fetch(mem); uint16_t r=(uint16_t)a-n;
                     flags_sub(n,r); mnemonic="CP 0x"+to_hex(n,2); break; }
        case 0xB8: { uint16_t r=(uint16_t)a-b; flags_sub(b,r); mnemonic="CP B"; break; }
        case 0xB9: { uint16_t r=(uint16_t)a-c; flags_sub(c,r); mnemonic="CP C"; break; }
        case 0xBB: { uint16_t r=(uint16_t)a-e; flags_sub(e,r); mnemonic="CP E"; break; }

        // === AND r/n ===
        case 0xA7: { a=a&a; flags_and(a); mnemonic="AND A"; break; }
        case 0xA0: { a=a&b; flags_and(a); mnemonic="AND B"; break; }
        case 0xA1: { a=a&c; flags_and(a); mnemonic="AND C"; break; }
        case 0xE6: { uint8_t n=fetch(mem); a=a&n; flags_and(a);
                     mnemonic="AND 0x"+to_hex(n,2); break; }

        // === OR r/n ===
        case 0xB7: { a=a|a; flags_or_xor(a); mnemonic="OR A"; break; }
        case 0xB0: { a=a|b; flags_or_xor(a); mnemonic="OR B"; break; }
        case 0xB1: { a=a|c; flags_or_xor(a); mnemonic="OR C"; break; }
        case 0xF6: { uint8_t n=fetch(mem); a=a|n; flags_or_xor(a);
                     mnemonic="OR 0x"+to_hex(n,2); break; }

        // === XOR r/n ===
        case 0xAF: { a=a^a; flags_or_xor(a); mnemonic="XOR A"; break; }
        case 0xA8: { a=a^b; flags_or_xor(a); mnemonic="XOR B"; break; }
        case 0xA9: { a=a^c; flags_or_xor(a); mnemonic="XOR C"; break; }
        case 0xEE: { uint8_t n=fetch(mem); a=a^n; flags_or_xor(a);
                     mnemonic="XOR 0x"+to_hex(n,2); break; }

        // === ADD HL, rr (16-bit — updates C and N only) ===
        case 0x09: { uint32_t r=(uint32_t)HL()+BC();
                     setFlag(FLAG_C,r>0xFFFF); setFlag(FLAG_N,false);
                     setHL((uint16_t)r); mnemonic="ADD HL, BC"; break; }
        case 0x19: { uint32_t r=(uint32_t)HL()+DE();
                     setFlag(FLAG_C,r>0xFFFF); setFlag(FLAG_N,false);
                     setHL((uint16_t)r); mnemonic="ADD HL, DE"; break; }
        case 0x29: { uint32_t r=(uint32_t)HL()+HL();
                     setFlag(FLAG_C,r>0xFFFF); setFlag(FLAG_N,false);
                     setHL((uint16_t)r); mnemonic="ADD HL, HL"; break; }
        case 0x39: { uint32_t r=(uint32_t)HL()+(uint32_t)sp;
                     setFlag(FLAG_C,r>0xFFFF); setFlag(FLAG_N,false);
                     setHL((uint16_t)r); mnemonic="ADD HL, SP"; break; }

        // === Unconditional Jumps ===
        case 0xC3: pc = fetch16(mem); mnemonic = "JP 0x" + to_hex(pc); break;
        case 0x18: { int8_t o = static_cast<int8_t>(fetch(mem));
                     pc = static_cast<uint16_t>(static_cast<int32_t>(pc) + o);
                     mnemonic = "JR 0x" + to_hex(pc); break; }

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
        case 0xCD: { uint16_t t = fetch16(mem);
                     mnemonic = "CALL 0x" + to_hex(t);
                     push16(mem, pc); pc = t; break; }
        case 0xC9: pc = pop16(mem); mnemonic = "RET"; break;

        // === ED prefix ===
        case 0xED: step_ED(mem, trace, current_pc); return;

        default:
            mnemonic = "UNKNOWN: 0x" + to_hex(opcode, 2);
            halted = true;
            break;
        }

        // --- Trace output ---
        std::stringstream ss;
        ss << "[" << to_hex(current_pc) << "] "
           << std::left << std::setw(30) << mnemonic
           << " | A:" << to_hex(a, 2)
           << " BC:" << to_hex(BC(), 4)
           << " DE:" << to_hex(DE(), 4)
           << " HL:" << to_hex(HL(), 4)
           << " F:" << std::bitset<8>(f)
           << " [Z:" << (flagZ() ? '1' : '0')
           << " C:" << (flagC() ? '1' : '0') << "]";
        log(trace, ss.str());
    }
};

// ---------------------------------------------------------------------------
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

    // Derive trace path: programs/lesson8.bin -> programs/lesson8.trace
    std::string tracePath = binPath;
    size_t lastDot = tracePath.find_last_of(".");
    if (lastDot != std::string::npos) tracePath = tracePath.substr(0, lastDot);
    tracePath += ".trace";

    std::ofstream traceFile(tracePath);
    if (!traceFile.is_open()) return 1;

    cpu.log(traceFile, "--- WPS-Z80 NEURAL TRACE: " + binPath + " ---");

    int cycles = 0;
    const int MAX_CYCLES = 50000;

    while (!cpu.halted && cycles < MAX_CYCLES) {
        cpu.step(mem, traceFile);
        cycles++;
    }

    if (cycles >= MAX_CYCLES) cpu.log(traceFile, "--- Safety Limit Reached ---");

    // Derive VRAM folder: programs/lesson8.bin -> programs/lesson8_vram/
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