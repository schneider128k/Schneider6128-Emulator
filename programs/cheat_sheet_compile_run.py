"""
g++ main.cpp -o emulator.exe -std=c++17
g++ monitor.cpp -o monitor.exe -I SDL3/include -L SDL3/lib -lSDL3 -std=c++17
python programs\gen_lesson9.py
Remove-Item -Recurse -Force programs\lesson9_vram -ErrorAction SilentlyContinue
Start-Process -FilePath ".\monitor.exe" -ArgumentList "programs\lesson9_vram --mode 0"
.\emulator.exe programs\lesson9.bin
"""
