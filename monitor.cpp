#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstring>

namespace fs = std::filesystem;

constexpr int CPC_W    = 160;
constexpr int CPC_H    = 200;
constexpr int SCALE_X  = 6;
constexpr int SCALE_Y  = 3;
constexpr int WINDOW_W = CPC_W * SCALE_X;
constexpr int WINDOW_H = CPC_H * SCALE_Y;
constexpr int VRAM_SIZE  = 0x4000;
constexpr int POLL_MS    = 200;
constexpr int WAIT_MS    = 500;

struct Colour { uint8_t r, g, b; };

constexpr Colour PALETTE_MODE0[16] = {
    {  0,   0,   0}, {  0,   0, 128}, {  0,   0, 255}, {128,   0,   0},
    {128,   0, 128}, {128,   0, 255}, {255,   0,   0}, {255,   0, 128},
    {255,   0, 255}, {  0, 128,   0}, {  0, 128, 128}, {  0, 128, 255},
    {128, 128,   0}, {128, 128, 128}, {128, 128, 255}, {  0, 255,   0},
};
constexpr Colour PALETTE_MODE1[4] = {
    {0,0,0}, {255,255,0}, {0,255,255}, {255,255,255},
};
constexpr Colour PALETTE_MODE2[2] = {
    {0,0,0}, {255,255,255},
};

inline int crtc_offset(int y, int x) {
    return (y % 8) * 0x0800 + (y / 8) * 0x0050 + x;
}

inline void decode_mode0(uint8_t b, uint8_t pens[2]) {
    pens[0] = ((b>>7)&1) | (((b>>3)&1)<<1) | (((b>>5)&1)<<2) | (((b>>1)&1)<<3);
    pens[1] = ((b>>6)&1) | (((b>>2)&1)<<1) | (((b>>4)&1)<<2) | (((b>>0)&1)<<3);
}
inline void decode_mode1(uint8_t b, uint8_t pens[4]) {
    for (int n = 0; n < 4; ++n)
        pens[n] = (((b >> (7-n)) & 1) << 1) | ((b >> (3-n)) & 1);
}
inline void decode_mode2(uint8_t b, uint8_t pens[8]) {
    for (int n = 0; n < 8; ++n)
        pens[n] = (b >> (7-n)) & 1;
}

bool load_vram(const std::string& path, std::vector<uint8_t>& vram) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    vram.assign(std::istreambuf_iterator<char>(f), {});
    return (int)vram.size() == VRAM_SIZE;
}

std::vector<std::string> collect_vram_files(const std::string& folder) {
    std::vector<std::string> files;
    try {
        for (const auto& entry : fs::directory_iterator(folder))
            if (entry.path().extension() == ".vram")
                files.push_back(entry.path().string());
    } catch (...) {}
    std::sort(files.begin(), files.end(), [](const std::string& a, const std::string& b) {
        return fs::path(a).filename().string() < fs::path(b).filename().string();
    });
    return files;
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

// Decode VRAM to flat RGB array (WINDOW_W * WINDOW_H * 3 bytes)
void vram_to_rgb(const std::vector<uint8_t>& vram, int mode,
                 std::vector<uint8_t>& rgb)
{
    rgb.assign((size_t)WINDOW_W * WINDOW_H * 3, 0);
    auto set_px = [&](int sx, int sy, const Colour& c) {
        if (sx < 0 || sx >= WINDOW_W || sy < 0 || sy >= WINDOW_H) return;
        int i = (sy * WINDOW_W + sx) * 3;
        rgb[i]=c.r; rgb[i+1]=c.g; rgb[i+2]=c.b;
    };
    if (mode == 0) {
        for (int y = 0; y < CPC_H; ++y)
            for (int x = 0; x < 80; ++x) {
                int idx = crtc_offset(y, x);
                if (idx >= VRAM_SIZE) continue;
                uint8_t pens[2]; decode_mode0(vram[idx], pens);
                for (int p = 0; p < 2; ++p) {
                    const Colour& c = PALETTE_MODE0[pens[p]&0xF];
                    int bsx=(x*2+p)*SCALE_X, bsy=y*SCALE_Y;
                    for (int dy=0;dy<SCALE_Y;++dy)
                        for (int dx=0;dx<SCALE_X;++dx)
                            set_px(bsx+dx,bsy+dy,c);
                }
            }
    } else if (mode == 1) {
        constexpr int PX_W = SCALE_X/2;
        for (int y = 0; y < CPC_H; ++y)
            for (int x = 0; x < 80; ++x) {
                int idx = crtc_offset(y, x);
                if (idx >= VRAM_SIZE) continue;
                uint8_t pens[4]; decode_mode1(vram[idx], pens);
                for (int p = 0; p < 4; ++p) {
                    const Colour& c = PALETTE_MODE1[pens[p]&0x3];
                    int bsx=(x*4+p)*PX_W, bsy=y*SCALE_Y;
                    for (int dy=0;dy<SCALE_Y;++dy)
                        for (int dx=0;dx<PX_W;++dx)
                            set_px(bsx+dx,bsy+dy,c);
                }
            }
    } else {
        for (int y = 0; y < CPC_H; ++y)
            for (int x = 0; x < 80; ++x) {
                int idx = crtc_offset(y, x);
                if (idx >= VRAM_SIZE) continue;
                uint8_t pens[8]; decode_mode2(vram[idx], pens);
                for (int p = 0; p < 8; ++p) {
                    const Colour& c = PALETTE_MODE2[pens[p]&0x1];
                    int bsx=(int)((float)(x*8+p)*SCALE_X/4.f);
                    int nsx=(int)((float)(x*8+p+1)*SCALE_X/4.f);
                    int bsy=y*SCALE_Y;
                    for (int dy=0;dy<SCALE_Y;++dy)
                        for (int sx=bsx;sx<nsx&&sx<WINDOW_W;++sx)
                            set_px(sx,bsy+dy,c);
                }
            }
    }
}

// Write 24-bit BMP (no external libraries needed)
// BMP is bottom-to-top, BGR order, rows padded to 4 bytes.
bool write_bmp(const std::string& path,
               const std::vector<uint8_t>& rgb,
               int width, int height)
{
    int row_bytes  = width * 3;
    int row_padded = (row_bytes + 3) & ~3;
    int padding    = row_padded - row_bytes;
    int px_size    = row_padded * height;
    uint8_t hdr[54]; memset(hdr, 0, 54);
    hdr[0]='B'; hdr[1]='M';
    uint32_t fsz=54+px_size;  memcpy(hdr+2,  &fsz,  4);
    uint32_t off=54;           memcpy(hdr+10, &off,  4);
    uint32_t dib=40;           memcpy(hdr+14, &dib,  4);
                               memcpy(hdr+18, &width,  4);
                               memcpy(hdr+22, &height, 4);
    uint16_t pl=1, bpp=24;    memcpy(hdr+26,&pl,2); memcpy(hdr+28,&bpp,2);
    uint32_t comp=0;           memcpy(hdr+30,&comp,4);
                               memcpy(hdr+34,&px_size,4);
    uint32_t ppm=3780;         memcpy(hdr+38,&ppm,4); memcpy(hdr+42,&ppm,4);
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    f.write(reinterpret_cast<const char*>(hdr), 54);
    uint8_t pad[3]={0,0,0};
    for (int y = height-1; y >= 0; --y) {
        const uint8_t* row = rgb.data() + y*width*3;
        for (int x = 0; x < width; ++x) {
            uint8_t bgr[3]={row[x*3+2],row[x*3+1],row[x*3+0]};
            f.write(reinterpret_cast<const char*>(bgr), 3);
        }
        if (padding) f.write(reinterpret_cast<const char*>(pad), padding);
    }
    return f.good();
}

void render_frame(SDL_Renderer* renderer,
                  const std::vector<uint8_t>& vram, int mode)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (mode == 0) {
        for (int y=0;y<CPC_H;++y)
            for (int x=0;x<80;++x) {
                int idx=crtc_offset(y,x); if(idx>=VRAM_SIZE)continue;
                uint8_t pens[2]; decode_mode0(vram[idx],pens);
                for (int p=0;p<2;++p) {
                    const Colour& c=PALETTE_MODE0[pens[p]&0xF];
                    SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,255);
                    SDL_FRect r={(float)((x*2+p)*SCALE_X),(float)(y*SCALE_Y),(float)SCALE_X,(float)SCALE_Y};
                    SDL_RenderFillRect(renderer,&r);
                }
            }
    } else if (mode==1) {
        constexpr int PX_W=SCALE_X/2;
        for (int y=0;y<CPC_H;++y)
            for (int x=0;x<80;++x) {
                int idx=crtc_offset(y,x); if(idx>=VRAM_SIZE)continue;
                uint8_t pens[4]; decode_mode1(vram[idx],pens);
                for (int p=0;p<4;++p) {
                    const Colour& c=PALETTE_MODE1[pens[p]&0x3];
                    SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,255);
                    SDL_FRect r={(float)((x*4+p)*PX_W),(float)(y*SCALE_Y),(float)PX_W,(float)SCALE_Y};
                    SDL_RenderFillRect(renderer,&r);
                }
            }
    } else {
        constexpr float PX_W=(float)SCALE_X/4.f;
        for (int y=0;y<CPC_H;++y)
            for (int x=0;x<80;++x) {
                int idx=crtc_offset(y,x); if(idx>=VRAM_SIZE)continue;
                uint8_t pens[8]; decode_mode2(vram[idx],pens);
                for (int p=0;p<8;++p) {
                    const Colour& c=PALETTE_MODE2[pens[p]&0x1];
                    SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,255);
                    SDL_FRect r={(float)(x*8+p)*PX_W,(float)(y*SCALE_Y),PX_W,(float)SCALE_Y};
                    SDL_RenderFillRect(renderer,&r);
                }
            }
    }
    SDL_RenderPresent(renderer);
}

void set_title(SDL_Window* w, int mode, const std::string& fname,
               bool play, int fi, int total, int fps, bool loop)
{
    std::string md[]={"Mode 0  160x200  16 colours","Mode 1  320x200   4 colours","Mode 2  640x200   2 colours"};
    std::string t="WPS-Z80 Monitor  |  "+md[mode]+"  |  "+fname;
    if (play&&total>0) {
        t+="  ["+std::to_string(fi+1)+"/"+std::to_string(total)+"]";
        t+="  "+std::to_string(fps)+" fps";
        if (loop) t+="  LOOP";
    }
    SDL_SetWindowTitle(w,t.c_str());
}

// Export mode — no SDL window
int run_export(const std::string& vram_folder,
               const std::string& out_folder, int mode)
{
    std::string md[]={"Mode 0  160x200  16 colours","Mode 1  320x200   4 colours","Mode 2  640x200   2 colours"};
    if (!fs::exists(vram_folder)) {
        std::cerr<<"Error: vram folder not found: "<<vram_folder<<"\n"; return 1;
    }
    auto files = collect_vram_files(vram_folder);
    if (files.empty()) {
        std::cerr<<"No .vram files in: "<<vram_folder<<"\n"; return 1;
    }
    fs::create_directories(out_folder);
    std::cout<<"WPS Monitor — Export\n";
    std::cout<<"Input:  "<<vram_folder<<"  ("<<files.size()<<" frames)\n";
    std::cout<<"Output: "<<out_folder<<"\n";
    std::cout<<md[mode]<<"  ->  "<<WINDOW_W<<"x"<<WINDOW_H<<" BMP\n";
    std::cout<<"Exporting";
    std::vector<uint8_t> vram, rgb;
    int exported=0;
    for (auto& vp : files) {
        if (!load_vram(vp,vram)) { std::cerr<<"\nCould not load "<<vp<<"\n"; continue; }
        vram_to_rgb(vram,mode,rgb);
        std::string stem=fs::path(vp).stem().string();
        if (!write_bmp(out_folder+"/"+stem+".bmp",rgb,WINDOW_W,WINDOW_H))
            { std::cerr<<"\nCould not write "<<stem<<".bmp\n"; continue; }
        ++exported;
        if (exported%8==0) std::cout<<"."<<std::flush;
    }
    std::cout<<" done.\n";
    std::cout<<"Exported "<<exported<<" BMP file(s) to "<<out_folder<<"\n\n";

    // Print ready-to-use ffmpeg commands
    std::cout<<"--- ffmpeg commands (PowerShell) ---\n\n";
    std::cout<<"Animated GIF for README:\n";
    std::cout<<"  ffmpeg -framerate 12 -i \""<<out_folder<<"\\frame_%04d.bmp\" `\n";
    std::cout<<"    -vf \"scale=480:-1:flags=lanczos,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse\" `\n";
    std::cout<<"    -loop 0 lesson10.gif\n\n";
    std::cout<<"MP4 for GitHub releases/issues:\n";
    std::cout<<"  ffmpeg -framerate 12 -i \""<<out_folder<<"\\frame_%04d.bmp\" `\n";
    std::cout<<"    -vf scale=480:-1 -c:v libx264 -pix_fmt yuv420p lesson10.mp4\n\n";
    std::cout<<"Tip: change -framerate and scale (480:-1 = half size, 960:-1 = full) as desired.\n";
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr<<"Usage: monitor <vram_folder> [--mode 0|1|2] [--play] [--fps N] [--loop] [--export-png <dir>]\n";
        std::cerr<<"  --mode N           Video mode 0/1/2. Default: 1\n";
        std::cerr<<"  --play             Playback mode\n";
        std::cerr<<"  --fps N            Playback fps (1-60). Default: 12\n";
        std::cerr<<"  --loop             Loop playback\n";
        std::cerr<<"  --export-png <dir> Export all frames as BMP to <dir> (no window)\n";
        std::cerr<<"  Keys: Escape=quit  Space=step (playback)\n";
        return 1;
    }

    std::string vram_folder=argv[1];
    int mode=1; bool play_mode=false,looping=false,do_export=false;
    int fps=12; std::string export_dir;

    for (int i=2;i<argc;++i) {
        std::string a=argv[i];
        if      (a=="--mode"       &&i+1<argc) mode=std::stoi(argv[++i]);
        else if (a=="--play")                  play_mode=true;
        else if (a=="--fps"        &&i+1<argc) {fps=std::stoi(argv[++i]);fps=std::max(1,std::min(60,fps));}
        else if (a=="--loop")                  looping=true;
        else if (a=="--export-png" &&i+1<argc) {do_export=true;export_dir=argv[++i];}
    }
    if (mode<0||mode>2){std::cerr<<"Invalid mode.\n";return 1;}
    if (do_export) return run_export(vram_folder,export_dir,mode);

    std::string md[]={"Mode 0  160x200  16 colours","Mode 1  320x200   4 colours","Mode 2  640x200   2 colours"};
    if (!SDL_Init(SDL_INIT_VIDEO)){std::cerr<<"SDL_Init failed: "<<SDL_GetError()<<"\n";return 1;}
    SDL_Window* window=SDL_CreateWindow(("WPS-Z80 Monitor  |  "+md[mode]).c_str(),WINDOW_W,WINDOW_H,0);
    if (!window){std::cerr<<"SDL_CreateWindow failed: "<<SDL_GetError()<<"\n";SDL_Quit();return 1;}
    SDL_Renderer* renderer=SDL_CreateRenderer(window,nullptr);
    if (!renderer){std::cerr<<"SDL_CreateRenderer failed: "<<SDL_GetError()<<"\n";SDL_DestroyWindow(window);SDL_Quit();return 1;}
    SDL_SetRenderDrawColor(renderer,0,0,0,255);
    SDL_RenderClear(renderer); SDL_RenderPresent(renderer);
    std::cout<<"WPS Monitor  "<<md[mode]<<"\n";
    std::cout<<"Watching: "<<vram_folder<<"\n";
    if (play_mode) std::cout<<"PLAYBACK fps="<<fps<<(looping?"  LOOP":"")<<"\n";
    else           std::cout<<"LIVE-WATCH\n";
    std::cout<<"Keys: Escape=quit"<<(play_mode?"  Space=step":"")<<"\n";

    std::vector<uint8_t> vram;
    bool running=true;

    if (play_mode) {
        int wa=0;
        while(!fs::exists(vram_folder)&&wa<10){
            std::cout<<"Waiting for folder...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_MS));++wa;
        }
        if(!fs::exists(vram_folder)){
            std::cerr<<"Folder not found: "<<vram_folder<<"\n";
            SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 1;
        }
        auto files=collect_vram_files(vram_folder);
        if(files.empty()){
            std::cerr<<"No .vram files.\n";
            SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 1;
        }
        int total=(int)files.size(), fi=0;
        std::cout<<"Found "<<total<<" frame(s).\n";
        auto interval=std::chrono::microseconds(1000000/fps);
        auto next=std::chrono::steady_clock::now();
        if(load_vram(files[0],vram)){
            render_frame(renderer,vram,mode);
            set_title(window,mode,fs::path(files[0]).filename().string(),true,0,total,fps,looping);
        }
        while(running){
            bool step=false;
            SDL_Event ev;
            while(SDL_PollEvent(&ev)){
                if(ev.type==SDL_EVENT_QUIT)running=false;
                if(ev.type==SDL_EVENT_KEY_DOWN){
                    if(ev.key.key==SDLK_ESCAPE)running=false;
                    if(ev.key.key==SDLK_SPACE) step=true;
                }
            }
            auto now=std::chrono::steady_clock::now();
            if(now>=next||step){
                int ni=fi+1;
                if(ni>=total){
                    if(looping) ni=0;
                    else{next=now+interval;std::this_thread::sleep_for(std::chrono::milliseconds(16));continue;}
                }
                fi=ni;
                if(load_vram(files[fi],vram)){
                    render_frame(renderer,vram,mode);
                    set_title(window,mode,fs::path(files[fi]).filename().string(),true,fi,total,fps,looping);
                }
                next=now+interval;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
    } else {
        std::string last;
        while(running){
            SDL_Event ev;
            while(SDL_PollEvent(&ev)){
                if(ev.type==SDL_EVENT_QUIT)running=false;
                if(ev.type==SDL_EVENT_KEY_DOWN&&ev.key.key==SDLK_ESCAPE)running=false;
            }
            std::string cand=newest_vram(vram_folder);
            if(!cand.empty()&&cand!=last){
                if(load_vram(cand,vram)){
                    render_frame(renderer,vram,mode);
                    last=cand;
                    std::string fn=fs::path(cand).filename().string();
                    set_title(window,mode,fn,false,0,0,fps,false);
                    std::cout<<"Rendered: "<<fn<<"\n";
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(POLL_MS));
        }
    }
    SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    return 0;
}