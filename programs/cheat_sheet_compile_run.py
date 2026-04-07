"""
g++ main.cpp -o emulator.exe -std=c++17
g++ monitor.cpp -o monitor.exe -I SDL3/include -L SDL3/lib -lSDL3 -std=c++17
python programs/gen_lesson7.py

# Terminal 1
.\monitor.exe programs\lesson7_vram --mode 0

# Terminal 2
.\emulator.exe programs\lesson7.bin
"""