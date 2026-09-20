#!/usr/bin/env python3
from PIL import Image
from pathlib import Path
p=Path(__file__).resolve().parents[1]; src=Image.open(p/'assets/prg32_logo.png').convert('RGBA')
for n in (16,32,64,128,256,512,1024):src.resize((n,n),Image.Resampling.LANCZOS).save(p/'assets'/f'prg32_{n}.png')
(src.resize((256,256),Image.Resampling.LANCZOS)).save(p/'platform/android/res/drawable/prg32_logo.png')
(src.resize((1024,1024),Image.Resampling.LANCZOS)).save(p/'platform/ios/prg32_logo.png')
print('PNG icon sizes regenerated; run iconutil on macOS if you replace the source and need a new .icns.')
