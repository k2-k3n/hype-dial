#!/usr/bin/env python3
"""MIT. HYPE DIAL's boot splash: the genre accents flash past like the departure board, HYPE and DIAL slam in the
app's own block letters, then the cut to the attract screen the app opens on.

Frames come from tools/splash_frames.cpp (host clang++, the app's src/type.h). Writes src/splash_data.h (RSP1,
<= 24 KiB) and build/splash.gif decoded from those exact bytes.

  /opt/homebrew/bin/python3.11 tools/make_splash.py
"""
import os, subprocess, sys, tempfile
from pathlib import Path
import numpy as np
ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'build'   # previews; created when the script runs

import splash_rsp as S   # noqa: E402

SDK = Path(os.environ['RESONA_SDK'])
HOLDS = [160, 90, 90, 90, 380, 380, 700]   # black, three band flashes, HYPE, DIAL, attract


def main():
    tmp = Path(tempfile.mkdtemp()); exe = tmp / 'frames'
    subprocess.run(['clang++', '-std=c++17', '-O2', '-DRESONA_LAB_HOST', f'-I{SDK}', f'-I{ROOT / "src"}', '-include', 'cmath',
                    str(ROOT / 'tools' / 'splash_frames.cpp'), '-o', str(exe)], check=True)
    subprocess.run([str(exe), str(tmp / 'f.raw')], check=True)
    raw = np.fromfile(tmp / 'f.raw', np.uint16).reshape(-1, 172, 320)
    frames = list(zip(HOLDS, raw))
    for hold, f in frames: print(f'  frame {hold:4d} ms: {S.frame_bytes(f):6d} bytes')
    data = S.encode(frames, duration_ms=sum(HOLDS))
    S.write_header(data, ROOT / 'src' / 'splash_data.h')
    OUT.mkdir(exist_ok=True)
    S.preview(S.decode(data), OUT / 'splash.gif', scale=2)
    print(f'{len(data)} bytes of {S.MAX_BYTES}')


if __name__ == '__main__':
    main()
