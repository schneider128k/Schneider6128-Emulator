#include <iostream>
#include <cstdint>
#include <vector>
#include <iomanip>

// --- 1. THE MEMORY SYSTEM ---
/**
 * @brief Simulates the 64KB RAM of the Schneider 6128.
 * The Z80 CPU can address 65,536 individual "mailboxes" of 1 byte each.
 */
struct Memory {
    uint8_t ram[65536];

    void clear() {
        for (int i = 0; i < 65536; i++) ram[i] = 0;
    }

    uint8_t read(uint16_t address) { return ram[address]; }
    void write(uint16_t address, uint8_t value) { ram[address] = value; }
};

// --- 2. THE Z80 CPU ---
/**
 * @brief A simplified Z80 CPU Core.
 * This handles the logic of our emulator, including the internal registers
 * and the Fetch-Decode-Execute cycle.
 */
struct Z80CPU {
    uint8_t a;  // Accumulator (8-bit math)
    uint8_t f;  // Flags (Status register)
    uint16_t pc; // Program Counter (16-bit address pointer)

    Memory* bus; // Connection to the RAM

    // Z80 Flag Bit-Masks
    const uint8_t FLAG_S = 0x80; // Sign Flag
    const uint8_t FLAG_Z = 0x40; // Zero Flag

    void reset() {
        a = 0;
        f = 0;
        pc = 0x0000;
        std::cout << "[CPU] System Reset. PC at 0x0000" << std::endl;
    }

    /**
     * @brief Internal helper to set bits in the Flag register.
     */
    void setFlag(uint8_t flagMask, bool condition) {
        if (condition) f |= flagMask;
        else f &= ~flagMask;
    }

    /**
     * @brief FETCH: Grabs 1 byte from RAM and advances the PC.
     */
    uint8_t fetch() {
        uint8_t data = bus->read(pc);
        pc++;
        return data;
    }

    /**
     * @brief FETCH WORD: Grabs 2 bytes for a 16-bit address (Little-Endian).
     */
    uint16_t fetchWord() {
        uint8_t low = fetch();
        uint8_t high = fetch();
        return (static_cast<uint16_t>(high) << 8) | low;
    }

    /**
     * @brief STEP: The heart of the simulation.
     */
    void step() {
        uint8_t opcode = fetch();

        switch (opcode) {
            case 0x00: // NOP
                break;

            case 0x3E: { // LD A, n (Load immediate value into A)
                uint8_t value = fetch();
                a = value;
                std::cout << "[CPU] LD A, " << (int)a << std::endl;
                break;
            }

            case 0x3C: // INC A
                a++;
                setFlag(FLAG_Z, (a == 0));
                setFlag(FLAG_S, (a & 0x80));
                std::cout << "[CPU] INC A | A: " << (int)a << std::endl;
                break;

            case 0x3D: // DEC A
                a--;
                setFlag(FLAG_Z, (a == 0));
                setFlag(FLAG_S, (a & 0x80));
                std::cout << "[CPU] DEC A | A: " << (int)a << std::endl;
                break;

            case 0xC3: { // JP nn (Unconditional Jump)
                uint16_t target = fetchWord();
                pc = target;
                std::printf("[CPU] JP 0x%04X\n", target);
                break;
            }

            case 0xCA: { // JP Z, nn (Jump if Zero)
                uint16_t target = fetchWord();
                if (f & FLAG_Z) {
                    pc = target;
                    std::printf("[CPU] JP Z Met: Jumping to 0x%04X\n", target);
                } else {
                    std::printf("[CPU] JP Z Not Met: Continuing at 0x%04X\n", pc);
                }
                break;
            }

            default:
                std::printf("[CPU] Unknown Opcode 0x%02X at 0x%04X\n", opcode, pc - 1);
                break;
        }
    }
};

// --- 3. THE MAIN PROGRAM ---
int main() {
    Memory myRam;
    myRam.clear();

    Z80CPU myCpu;
    myCpu.bus = &myRam;
    myCpu.reset();

    /**
     * OUR PROGRAM: A 3-2-1 Countdown Loop
     * 0x0000: LD A, 3      (3E 03)
     * 0x0002: DEC A        (3D)     <-- Loop start
     * 0x0003: JP Z, 0x0009 (CA 09 00)
     * 0x0006: JP 0x0002    (C3 02 00)
     * 0x0009: NOP          (00)     <-- End
     */
    myRam.write(0x0000, 0x3E); myRam.write(0x0001, 0x03);
    myRam.write(0x0002, 0x3D);
    myRam.write(0x0003, 0xCA); myRam.write(0x0004, 0x09); myRam.write(0x0005, 0x00);
    myRam.write(0x0006, 0xC3); myRam.write(0x0007, 0x02); myRam.write(0x0008, 0x00);
    myRam.write(0x0009, 0x00);

    std::cout << "--- Starting Schneider Countdown ---" << std::endl;

    // Run up to 20 steps to avoid infinite loops if we made a mistake
    for (int i = 0; i < 20; i++) {
        myCpu.step();
        if (myCpu.pc == 0x000A) { // One byte past our NOP
            std::cout << "--- Program Finished Successfully ---" << std::endl;
            break;
        }
    }

    return 0;
}