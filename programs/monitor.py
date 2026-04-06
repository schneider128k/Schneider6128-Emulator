# monitor.py
# WPS-Z80 Live Monitor - Mode 1 (320x200, 4 colours)
#
# Usage:  python programs/monitor.py programs/lesson6_vram/
#
# Watches the given _vram/ folder for new .vram files and renders
# each one as an authentic CPC Mode 1 frame in a live pygame window.
# Leave this running while the emulator executes; it updates automatically.
#
# Requirements: pip install pygame
#
# CRTC address formula (Mode 1, VRAM base 0xC000):
#   For absolute pixel row y (0-199) and byte column x (0-79):
#     address = (y % 8) * 0x0800 + (y // 8) * 0x0050 + x
#   (offset from 0xC000, i.e. index into the 16KB dump)
#
# Mode 1 byte encoding (4 pixels per byte):
#   bit7  bit6  bit5  bit4  bit3  bit2  bit1  bit0
#   p0[1] p1[1] p2[1] p3[1] p0[0] p1[0] p2[0] p3[0]
#   pen_n = ((byte >> (7-n)) & 1) << 1 | ((byte >> (3-n)) & 1)

import pygame
import sys
import os
import time
import glob

# ---------------------------------------------------------------------------
# Authentic CPC Mode 1 default firmware palette (pens 0-3)
# Source: Amstrad firmware defaults, hardware RGB 3-level (0, 128, 255)
#   Pen 0 = firmware colour  1 = Black          (0,   0,   0)
#   Pen 1 = firmware colour 24 = Bright Yellow   (255, 255,   0)
#   Pen 2 = firmware colour 20 = Bright Cyan     (0,   255, 255)
#   Pen 3 = firmware colour  6 = Bright White    (255, 255, 255)
# ---------------------------------------------------------------------------
CPC_MODE1_PALETTE = [
    (0,   0,   0),    # pen 0 — black
    (255, 255,   0),  # pen 1 — bright yellow
    (0,   255, 255),  # pen 2 — bright cyan
    (255, 255, 255),  # pen 3 — bright white
]

SCREEN_W  = 320   # Mode 1 logical pixels
SCREEN_H  = 200
SCALE     = 3     # window = 960 x 600
POLL_MS   = 200   # folder poll interval (milliseconds)

def decode_mode1_byte(byte_val):
    """
    Decode one Mode 1 byte into 4 pen indices (left to right: p0, p1, p2, p3).

    Bit layout:
      bit7=p0[1]  bit6=p1[1]  bit5=p2[1]  bit4=p3[1]
      bit3=p0[0]  bit2=p1[0]  bit1=p2[0]  bit0=p3[0]

    pen_n = (high_bit << 1) | low_bit
    """
    pens = []
    for n in range(4):
        high_bit = (byte_val >> (7 - n)) & 1
        low_bit  = (byte_val >> (3 - n)) & 1
        pens.append((high_bit << 1) | low_bit)
    return pens

def vram_to_surface(vram_bytes, palette):
    """
    Convert a 16384-byte raw VRAM dump to a pygame Surface (320x200).

    CRTC interleaved layout:
      offset = (y % 8) * 0x0800 + (y // 8) * 0x0050 + x
    where x is the byte column (0-79) and y is the pixel row (0-199).
    Each byte encodes 4 horizontal pixels.
    """
    surface = pygame.Surface((SCREEN_W, SCREEN_H))
    pixel_array = pygame.PixelArray(surface)

    for y in range(SCREEN_H):
        char_line = y // 8
        pixel_row = y %  8
        row_base  = pixel_row * 0x0800 + char_line * 0x0050

        for x_byte in range(80):         # 80 bytes per row = 320 pixels
            offset = row_base + x_byte
            if offset >= len(vram_bytes):
                continue
            pens = decode_mode1_byte(vram_bytes[offset])
            for p in range(4):
                px = x_byte * 4 + p
                colour = palette[pens[p]]
                pixel_array[px][y] = surface.map_rgb(*colour)

    del pixel_array
    return surface

def get_sorted_vram_files(folder):
    """Return all .vram files in folder, sorted by name (frame_0001 < frame_0002...)."""
    return sorted(glob.glob(os.path.join(folder, "*.vram")))

def main():
    if len(sys.argv) < 2:
        print("Usage: python monitor.py <vram_folder>")
        print("Example: python monitor.py programs/lesson6_vram/")
        sys.exit(1)

    vram_folder = sys.argv[1]
    if not os.path.isdir(vram_folder):
        print(f"Waiting for folder to appear: {vram_folder}")

    pygame.init()
    window = pygame.display.set_mode((SCREEN_W * SCALE, SCREEN_H * SCALE))
    pygame.display.set_caption("WPS-Z80 Monitor — Mode 1 (320x200) — waiting...")
    clock = pygame.time.Clock()

    last_file    = None   # path of the last rendered file
    last_mtime   = 0      # mtime of last rendered file

    # Show a black screen while waiting for the first frame
    window.fill((0, 0, 0))
    pygame.display.flip()

    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            if event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
                running = False

        # Poll for new or updated .vram files
        if os.path.isdir(vram_folder):
            files = get_sorted_vram_files(vram_folder)
            if files:
                newest = files[-1]
                try:
                    mtime = os.path.getmtime(newest)
                except OSError:
                    mtime = 0

                if newest != last_file or mtime != last_mtime:
                    try:
                        with open(newest, "rb") as f:
                            vram_bytes = f.read()
                        if len(vram_bytes) == 0x4000:
                            surface = vram_to_surface(vram_bytes, CPC_MODE1_PALETTE)
                            scaled  = pygame.transform.scale(
                                surface, (SCREEN_W * SCALE, SCREEN_H * SCALE)
                            )
                            window.blit(scaled, (0, 0))
                            pygame.display.flip()
                            frame_name = os.path.basename(newest)
                            pygame.display.set_caption(
                                f"WPS-Z80 Monitor — Mode 1 — {frame_name} "
                                f"({len(files)} frame(s))"
                            )
                            last_file  = newest
                            last_mtime = mtime
                    except (OSError, IOError):
                        pass  # file still being written, retry next poll

        clock.tick(1000 // POLL_MS)

    pygame.quit()

if __name__ == "__main__":
    main()