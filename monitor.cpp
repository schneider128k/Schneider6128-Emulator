/**
 * WOCJAN PERCUSSIVE SYSTEMS - CPC Monitor
 * Milestone 6: Live Mode 1 Display (320x200, 4 colours)
 * -----------------------------------------------------------
 * Watches a _vram/ folder for .vram files and renders each
 * as an authentic CPC Mode 1 frame in a live SDL3 window.
 *
 * Usage:  monitor.exe programs/lesson6_vram/
 *
 * Build (Windows):
 *   g++ monitor.cpp -o monitor -I SDL3/include -L SDL3/lib/x64 -lSDL3
 * Build (Linux):
 *   g++ monitor.cpp -o monitor $(sdl3-config --cflags --libs)
 * Build (Mac):
 *   g++ monitor.cpp -o monitor $(sdl3-config --cflags --libs)
 *
 * No platform-specific code anywhere in this file.
 * SDL3 handles Win32 / Cocoa / X11 / Wayland behind the scenes.
 * -----------------------------------------------------------
 * CPC Mode 1 memory layout:
 *   VRAM base: 0xC000, size: 16KB (0x4000 bytes)
 *   1 byte = 4 pixels, 2 bits per pixel (pens 0-3)
 *   Bit layout per byte:
 *     bit7=p0[1] bit6=p1[1] bit5=p2[1] bit4=p3[1]
 *     bit3=p0[0] bit2=p1[0] bit1=p2[0] bit0=p3[0]
 *   CRTC address (offset from 0xC000):
 *     offset = (y % 8) * 0x0800 + (y / 8) * 0x0050 + x_byte
 *
 * Default Mode 1 firmware palette:
 *   Pen 0 = Black         (  0,   0,   0)
 *   Pen 1 = Bright Yellow (255, 255,   0)
 *   Pen 2 = Bright Cyan   (  0, 255, 255)
 *   Pen 3 = Bright White  (255, 255, 255)
 * -----------------------------------------------------------
 */

#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <thread>
#include <chrono>
#include <algorithm>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
constexpr int CPC_W        = 320;
constexpr int CPC_H        = 200;
constexpr int SCALE        = 3;          // window = 960 x 600
constexpr int WINDOW_W     = CPC_W * SCALE;
constexpr int WINDOW_H     = CPC_H * SCALE;
constexpr int VRAM_SIZE    = 0x4000;     // 16 KB
constexpr int POLL_MS      = 200;        // folder poll interval

// ---------------------------------------------------------------------------
// Authentic CPC Mode 1 default firmware palette
// 3-level RGB (0, 128, 255) — standard emulator approximation
// ---------------------------------------------------------------------------
struct Colour { uint8_t r, g, b; };

constexpr Colour PALETTE[4] = {
    {  0,   0,   0},   // pen 0 — black
    {255, 255,   0},   // pen 1 — bright yellow
    {  0, 255, 255},   // pen 2 — bright cyan
    {255, 255, 255},   // pen 3 — bright white
};

// ---------------------------------------------------------------------------
// Mode 1 decoder
// Decodes one VRAM byte into 4 pen indices (pixels left to right).
// pen_n = ((byte >> (7-n)) & 1) << 1 | ((byte >> (3-n)) & 1)
// ---------------------------------------------------------------------------
void decode_mode1_byte(uint8_t b, uint8_t pens[4]) {
    for (int n = 0; n < 4; ++n) {
        uint8_t hi = (b >> (7 - n)) & 1;
        uint8_t lo = (b >> (3 - n)) & 1;
        pens[n] = (hi << 1) | lo;
    }
}

// ---------------------------------------------------------------------------
// VRAM → pixel buffer
// Output: flat RGBA array, CPC_W * CPC_H * 4 bytes, row-major.
// ---------------------------------------------------------------------------
void vram_to_pixels(const std::vector<uint8_t>& vram,
                    std::vector<uint8_t>& pixels)
{
    pixels.resize(CPC_W * CPC_H * 4);

    for (int y = 0; y < CPC_H; ++y) {
        int offset = (y % 8) * 0x0800 + (y / 8) * 0x0050;
        for (int x = 0; x < 80; ++x) {          // 80 bytes = 320 pixels
            int idx = offset + x;
            if (idx >= VRAM_SIZE) continue;

            uint8_t pens[4];
            decode_mode1_byte(vram[idx], pens);

            for (int p = 0; p < 4; ++p) {
                int px   = x * 4 + p;
                int base = (y * CPC_W + px) * 4;
                const Colour& c = PALETTE[pens[p]];
                pixels[base + 0] = c.r;
                pixels[base + 1] = c.g;
                pixels[base + 2] = c.b;
                pixels[base + 3] = 255;          // alpha
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Folder watcher: returns the newest .vram file in a directory.
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
// Load a .vram file into a byte vector. Returns false on failure.
// ---------------------------------------------------------------------------
bool load_vram(const std::string& path, std::vector<uint8_t>& vram) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    vram.assign(std::istreambuf_iterator<char>(f), {});
    return vram.size() == VRAM_SIZE;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: monitor <vram_folder>\n";
        std::cerr << "Example: monitor programs/lesson6_vram/\n";
        return 1;
    }

    std::string vram_folder = argv[1];

    // --- SDL3 init ---
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "WPS-Z80 Monitor  |  Mode 1  |  320x200",
        WINDOW_W, WINDOW_H,
        0                          // no special flags needed
    );
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Texture: CPC logical resolution, scaled up by renderer
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        CPC_W, CPC_H
    );
    if (!texture) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << "\n";
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Disable texture interpolation — we want sharp CPC pixels
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    // --- State ---
    std::string      last_path;
    std::vector<uint8_t> vram, pixels;
    bool             running = true;

    // Show black screen while waiting
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    std::cout << "WPS Monitor running. Watching: " << vram_folder << "\n";
    std::cout << "Press Escape or close window to quit.\n";

    // --- Main loop ---
    while (running) {

        // Process SDL events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN &&
                event.key.key == SDLK_ESCAPE)   running = false;
        }

        // Poll for new .vram file
        std::string candidate = newest_vram(vram_folder);
        if (!candidate.empty() && candidate != last_path) {
            if (load_vram(candidate, vram)) {
                vram_to_pixels(vram, pixels);

                // Upload pixel buffer to GPU texture
                SDL_UpdateTexture(texture, nullptr,
                                  pixels.data(), CPC_W * 4);

                // Render texture scaled to full window
                SDL_RenderClear(renderer);
                SDL_RenderTexture(renderer, texture, nullptr, nullptr);
                SDL_RenderPresent(renderer);

                last_path = candidate;

                // Update title bar with current frame filename
                std::string fname = fs::path(candidate).filename().string();
                std::string title = "WPS-Z80 Monitor  |  Mode 1  |  " + fname;
                SDL_SetWindowTitle(window, title.c_str());

                std::cout << "Rendered: " << fname << "\n";
            }
        }

        // Yield to avoid busy-spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_MS));
    }

    // --- Cleanup ---
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}