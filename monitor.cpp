#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

/**
 * WOCJAN PERCUSSIVE SYSTEMS - CPC MONITOR
 * Milestone 9: Authentic CPC pixel aspect ratio
 * ----------------------------------------------------------------------------
 * The Schneider CPC 6128 displays on a monitor with a 4:3 aspect ratio.
 * Mode 0 has 160x200 logical pixels but each pixel is physically 2x wider
 * than tall on the real screen. We reproduce this with:
 *   SCALE_X = 6  (each logical x unit = 6 screen pixels)
 *   SCALE_Y = 3  (each logical y unit = 3 screen pixels)
 * This gives a 960x600 window matching authentic CPC proportions.
 *
 * Mode 0 decode (verified):
 *   pens[0] = (b>>7&1) | ((b>>3&1)<<1) | ((b>>5&1)<<2) | ((b>>1&1)<<3)
 *   pens[1] = (b>>6&1) | ((b>>2&1)<<1) | ((b>>4&1)<<2) | ((b>>0&1)<<3)
 *
 * Solid pen encoding (verified):
 *   pen 0=0x00 pen 1=0xC0 pen 2=0x0C pen 3=0xCC pen 4=0x30 pen 5=0xF0
 *   pen 6=0x3C pen 7=0xFC pen 8=0x03 pen 9=0xC3 pen10=0x0F pen11=0xCF
 *   pen12=0x33 pen13=0xF3 pen14=0x3F pen15=0xFF
 * ----------------------------------------------------------------------------
 */

// CPC logical resolution
constexpr int CPC_W    = 160;
constexpr int CPC_H    = 200;

// Authentic CPC pixel aspect ratio
// Each Mode 0 logical pixel is 2x wider than tall on real hardware.
// SCALE_X=6, SCALE_Y=3 gives 960x600 window.
constexpr int SCALE_X  = 6;
constexpr int SCALE_Y  = 3;
constexpr int WINDOW_W = CPC_W * SCALE_X;   // 960
constexpr int WINDOW_H = CPC_H * SCALE_Y;   // 600

constexpr int VRAM_SIZE = 0x4000;
constexpr int POLL_MS   = 200;

struct Colour { uint8_t r, g, b; };

// ---------------------------------------------------------------------------
// Mode 0 default palette — 16 pens
// CPC hardware uses 3-level RGB: 0=0, half=128, full=255
// ---------------------------------------------------------------------------
constexpr Colour PALETTE_MODE0[16] = {
    {  0,   0,   0},   // pen  0 — black
    {  0,   0, 128},   // pen  1 — blue
    {  0,   0, 255},   // pen  2 — bright blue
    {128,   0,   0},   // pen  3 — red
    {128,   0, 128},   // pen  4 — magenta
    {128,   0, 255},   // pen  5 — mauve
    {255,   0,   0},   // pen  6 — bright red
    {255,   0, 128},   // pen  7 — bright magenta
    {255,   0, 255},   // pen  8 — bright mauve
    {  0, 128,   0},   // pen  9 — green
    {  0, 128, 128},   // pen 10 — cyan
    {  0, 128, 255},   // pen 11 — sky blue
    {128, 128,   0},   // pen 12 — yellow
    {128, 128, 128},   // pen 13 — white (grey)
    {128, 128, 255},   // pen 14 — pastel blue
    {  0, 255,   0},   // pen 15 — bright green
};

// ---------------------------------------------------------------------------
// Mode 1 default palette — 4 pens
// ---------------------------------------------------------------------------
constexpr Colour PALETTE_MODE1[4] = {
    {  0,   0,   0},   // pen 0 — black
    {255, 255,   0},   // pen 1 — bright yellow
    {  0, 255, 255},   // pen 2 — bright cyan
    {255, 255, 255},   // pen 3 — bright white
};

// ---------------------------------------------------------------------------
// Mode 2 default palette — 2 pens
// ---------------------------------------------------------------------------
constexpr Colour PALETTE_MODE2[2] = {
    {  0,   0,   0},   // pen 0 — black
    {255, 255, 255},   // pen 1 — bright white
};

// ---------------------------------------------------------------------------
// CRTC address formula — identical for all three modes
// For pixel row y (0-199) and byte column x (0-79):
//   offset = (y % 8) * 0x0800 + (y / 8) * 0x0050 + x
// ---------------------------------------------------------------------------
inline int crtc_offset(int y, int x) {
    return (y % 8) * 0x0800 + (y / 8) * 0x0050 + x;
}

// ---------------------------------------------------------------------------
// Mode 0 decoder — verified against CPC hardware bit layout
//
// Bit layout: bit7=p0[0] bit6=p1[0] bit5=p0[2] bit4=p1[2]
//             bit3=p0[1] bit2=p1[1] bit1=p0[3] bit0=p1[3]
//
// Decode:
//   p0 = bit7 | (bit3<<1) | (bit5<<2) | (bit1<<3)
//   p1 = bit6 | (bit2<<1) | (bit4<<2) | (bit0<<3)
// ---------------------------------------------------------------------------
inline void decode_mode0(uint8_t b, uint8_t pens[2]) {
    pens[0] = ((b>>7)&1) | (((b>>3)&1)<<1) | (((b>>5)&1)<<2) | (((b>>1)&1)<<3);
    pens[1] = ((b>>6)&1) | (((b>>2)&1)<<1) | (((b>>4)&1)<<2) | (((b>>0)&1)<<3);
}

// ---------------------------------------------------------------------------
// Mode 1 decoder
//
// Bit layout: bit7=p0[1] bit6=p1[1] bit5=p2[1] bit4=p3[1]
//             bit3=p0[0] bit2=p1[0] bit1=p2[0] bit0=p3[0]
//
// Decode pixel n: pen = ((b>>(7-n))&1)<<1 | ((b>>(3-n))&1)
// ---------------------------------------------------------------------------
inline void decode_mode1(uint8_t b, uint8_t pens[4]) {
    for (int n = 0; n < 4; ++n) {
        uint8_t hi = (b >> (7-n)) & 1;
        uint8_t lo = (b >> (3-n)) & 1;
        pens[n] = (hi << 1) | lo;
    }
}

// ---------------------------------------------------------------------------
// Mode 2 decoder — 8 pixels per byte, MSB first, 1 bit each
// ---------------------------------------------------------------------------
inline void decode_mode2(uint8_t b, uint8_t pens[8]) {
    for (int n = 0; n < 8; ++n)
        pens[n] = (b >> (7-n)) & 1;
}

// ---------------------------------------------------------------------------
// VRAM folder watcher — returns path of newest .vram file in folder
// ---------------------------------------------------------------------------
std::string newest_vram(const std::string& folder) {
    std::string newest_path;
    fs::file_time_type newest_time;
    try {
        for (const auto& entry : fs::directory_iterator(folder)) {
            if (entry.path().extension() == ".vram") {
                auto t = entry.last_write_time();
                if (newest_path.empty() || t > newest_time) {
                    newest_time = t;
                    newest_path = entry.path().string();
                }
            }
        }
    } catch (...) {}
    return newest_path;
}

// ---------------------------------------------------------------------------
// Load a .vram file into a buffer
// ---------------------------------------------------------------------------
bool load_vram(const std::string& path, std::vector<uint8_t>& vram) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    vram.assign(std::istreambuf_iterator<char>(f), {});
    return (int)vram.size() == VRAM_SIZE;
}

// ---------------------------------------------------------------------------
// Render one VRAM frame
//
// Coordinate system:
//   SDL_SetRenderScale is NOT used — we draw explicit SDL_FRect rectangles
//   in screen pixels so we can use different x and y scales.
//
// Mode 0: 160 logical pixels wide, 80 bytes per row, 2 pixels per byte.
//   Each logical pixel renders as SCALE_X x SCALE_Y screen pixels.
//   Byte column x, pixel index p (0 or 1):
//     screen_x = (x * 2 + p) * SCALE_X
//     screen_y = y * SCALE_Y
//     rect width  = SCALE_X
//     rect height = SCALE_Y
//
// Mode 1: 320 logical pixels wide, 80 bytes per row, 4 pixels per byte.
//   Each logical pixel renders as (SCALE_X/2) x SCALE_Y screen pixels.
//   (SCALE_X=6 → 3 screen pixels wide per Mode 1 pixel)
//
// Mode 2: 640 logical pixels wide, 80 bytes per row, 8 pixels per byte.
//   Each logical pixel renders as (SCALE_X/4) x SCALE_Y screen pixels.
//   (SCALE_X=6 → 1.5 screen pixels — rounded to nearest integer)
// ---------------------------------------------------------------------------
void render_frame(SDL_Renderer* renderer,
                  const std::vector<uint8_t>& vram,
                  int mode)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (mode == 0) {
        // Mode 0: 160x200, 16 colours, 2 pixels per byte
        // Each pixel: SCALE_X wide x SCALE_Y tall in screen coords
        for (int y = 0; y < CPC_H; ++y) {
            for (int x = 0; x < 80; ++x) {
                int idx = crtc_offset(y, x);
                if (idx >= VRAM_SIZE) continue;
                uint8_t pens[2];
                decode_mode0(vram[idx], pens);
                for (int p = 0; p < 2; ++p) {
                    const Colour& c = PALETTE_MODE0[pens[p] & 0xF];
                    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
                    SDL_FRect rect = {
                        (float)((x * 2 + p) * SCALE_X),
                        (float)(y * SCALE_Y),
                        (float)SCALE_X,
                        (float)SCALE_Y
                    };
                    SDL_RenderFillRect(renderer, &rect);
                }
            }
        }
    }
    else if (mode == 1) {
        // Mode 1: 320x200, 4 colours, 4 pixels per byte
        // Each pixel: (SCALE_X/2) wide x SCALE_Y tall = 3x3 screen pixels
        constexpr int PX_W = SCALE_X / 2;   // = 3
        for (int y = 0; y < CPC_H; ++y) {
            for (int x = 0; x < 80; ++x) {
                int idx = crtc_offset(y, x);
                if (idx >= VRAM_SIZE) continue;
                uint8_t pens[4];
                decode_mode1(vram[idx], pens);
                for (int p = 0; p < 4; ++p) {
                    const Colour& c = PALETTE_MODE1[pens[p] & 0x3];
                    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
                    SDL_FRect rect = {
                        (float)((x * 4 + p) * PX_W),
                        (float)(y * SCALE_Y),
                        (float)PX_W,
                        (float)SCALE_Y
                    };
                    SDL_RenderFillRect(renderer, &rect);
                }
            }
        }
    }
    else if (mode == 2) {
        // Mode 2: 640x200, 2 colours, 8 pixels per byte
        // Each pixel: (SCALE_X/4) wide x SCALE_Y tall = 1.5 screen pixels
        // Use float for sub-pixel accuracy
        constexpr float PX_W = (float)SCALE_X / 4.0f;   // = 1.5f
        for (int y = 0; y < CPC_H; ++y) {
            for (int x = 0; x < 80; ++x) {
                int idx = crtc_offset(y, x);
                if (idx >= VRAM_SIZE) continue;
                uint8_t pens[8];
                decode_mode2(vram[idx], pens);
                for (int p = 0; p < 8; ++p) {
                    const Colour& c = PALETTE_MODE2[pens[p] & 0x1];
                    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
                    SDL_FRect rect = {
                        (float)(x * 8 + p) * PX_W,
                        (float)(y * SCALE_Y),
                        PX_W,
                        (float)SCALE_Y
                    };
                    SDL_RenderFillRect(renderer, &rect);
                }
            }
        }
    }

    SDL_RenderPresent(renderer);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: monitor <vram_folder> [--mode 0|1|2]\n";
        std::cerr << "  --mode 0   Mode 0: 160x200, 16 colours (default for lesson9+)\n";
        std::cerr << "  --mode 1   Mode 1: 320x200,  4 colours\n";
        std::cerr << "  --mode 2   Mode 2: 640x200,  2 colours\n";
        std::cerr << "\nWindow: " << WINDOW_W << "x" << WINDOW_H
                  << " (authentic CPC pixel aspect ratio " << SCALE_X << ":" << SCALE_Y << ")\n";
        return 1;
    }

    std::string vram_folder = argv[1];

    // Parse --mode argument (default: 1)
    int mode = 1;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc)
            mode = std::stoi(argv[++i]);
    }

    if (mode < 0 || mode > 2) {
        std::cerr << "Invalid mode. Use 0, 1, or 2.\n";
        return 1;
    }

    std::string mode_desc[] = {
        "Mode 0  160x200  16 colours",
        "Mode 1  320x200   4 colours",
        "Mode 2  640x200   2 colours"
    };

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    std::string title = "WPS-Z80 Monitor  |  " + mode_desc[mode];
    SDL_Window* window = SDL_CreateWindow(
        title.c_str(), WINDOW_W, WINDOW_H, 0);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        SDL_Quit(); return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }

    // No SDL_SetRenderScale — we handle scaling manually in render_frame
    // so we can use different x and y scale factors.

    std::string last_path;
    std::vector<uint8_t> vram;
    bool running = true;

    // Clear to black on startup
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    std::cout << "WPS Monitor running.\n";
    std::cout << "Watching: " << vram_folder << "\n";
    std::cout << mode_desc[mode] << "\n";
    std::cout << "Window: " << WINDOW_W << "x" << WINDOW_H
              << "  (CPC authentic aspect ratio)\n";
    std::cout << "Press Escape or close window to quit.\n";

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT)   running = false;
            if (event.type == SDL_EVENT_KEY_DOWN &&
                event.key.key == SDLK_ESCAPE)   running = false;
        }

        std::string candidate = newest_vram(vram_folder);
        if (!candidate.empty() && candidate != last_path) {
            if (load_vram(candidate, vram)) {
                render_frame(renderer, vram, mode);
                last_path = candidate;
                std::string fname = fs::path(candidate).filename().string();
                SDL_SetWindowTitle(window,
                    ("WPS-Z80 Monitor  |  " + mode_desc[mode] +
                     "  |  " + fname).c_str());
                std::cout << "Rendered: " << fname
                          << " (mode " << mode << ")\n";
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_MS));
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}