#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

constexpr int CPC_W    = 320;
constexpr int CPC_H    = 200;
constexpr int SCALE    = 3;
constexpr int WINDOW_W = CPC_W * SCALE;
constexpr int WINDOW_H = CPC_H * SCALE;
constexpr int VRAM_SIZE = 0x4000;
constexpr int POLL_MS  = 200;

struct Colour { uint8_t r, g, b; };

constexpr Colour PALETTE[4] = {
    {  0,   0,   0},   // pen 0 — black
    {255, 255,   0},   // pen 1 — bright yellow
    {  0, 255, 255},   // pen 2 — bright cyan
    {255, 255, 255},   // pen 3 — bright white
};

void decode_mode1_byte(uint8_t b, uint8_t pens[4]) {
    for (int n = 0; n < 4; ++n) {
        uint8_t hi = (b >> (7 - n)) & 1;
        uint8_t lo = (b >> (3 - n)) & 1;
        pens[n] = (hi << 1) | lo;
    }
}

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

bool load_vram(const std::string& path, std::vector<uint8_t>& vram) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    vram.assign(std::istreambuf_iterator<char>(f), {});
    return vram.size() == VRAM_SIZE;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: monitor <vram_folder>\n";
        return 1;
    }

    std::string vram_folder = argv[1];

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "WPS-Z80 Monitor  |  Mode 1  |  320x200",
        WINDOW_W, WINDOW_H, 0
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

    // Scale up: each CPC pixel = SCALE x SCALE screen pixels
    SDL_SetRenderScale(renderer, (float)SCALE, (float)SCALE);

    std::string last_path;
    std::vector<uint8_t> vram;
    bool running = true;

    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    std::cout << "WPS Monitor running. Watching: " << vram_folder << "\n";
    std::cout << "Press Escape or close window to quit.\n";

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN &&
                event.key.key == SDLK_ESCAPE) running = false;
        }

        std::string candidate = newest_vram(vram_folder);
        if (!candidate.empty() && candidate != last_path) {
            if (load_vram(candidate, vram)) {

                SDL_RenderClear(renderer);

                for (int y = 0; y < CPC_H; ++y) {
                    int row_offset = (y % 8) * 0x0800 + (y / 8) * 0x0050;
                    for (int x = 0; x < 80; ++x) {
                        int idx = row_offset + x;
                        if (idx >= VRAM_SIZE) continue;

                        uint8_t pens[4];
                        decode_mode1_byte(vram[idx], pens);

                        for (int p = 0; p < 4; ++p) {
                            const Colour& c = PALETTE[pens[p]];
                            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
                            SDL_FRect rect = {
                                (float)(x * 4 + p),
                                (float)y,
                                1.0f,
                                1.0f
                            };
                            SDL_RenderFillRect(renderer, &rect);
                        }
                    }
                }

                SDL_RenderPresent(renderer);
                last_path = candidate;

                std::string fname = fs::path(candidate).filename().string();
                SDL_SetWindowTitle(window,
                    ("WPS-Z80 Monitor  |  Mode 1  |  " + fname).c_str());
                std::cout << "Rendered: " << fname << "\n";
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_MS));
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}