---
description: Convert all PGN games in the Games directory and start engine tuning
---

This workflow automates the process of preparing tuning data and optimizing the IronRook evaluation.

### Prerequisites
- Python installed with `python-chess` library
- Engine binary built at `build/IronRook.exe`

### Steps

1. **Rebuild the Engine** (Ensure the latest fixes are included)
// turbo
```bash
cd build
mingw32-make
cd ..
```

2. **Convert all PGN games to EPD**
Run the conversion script to generate `Games/tuning_data_all.epd`.
// turbo
```bash
python tools/convert_all_pgns.py
```

3. **Tune the Engine**
Launch the engine and perform 10,000 tuning iterations on the new dataset.
// turbo
```bash
echo "tune Games/tuning_data_all.epd 10000" | ./build/IronRook.exe
```

4. **Verify and Save Results**
The tuned parameters will be printed to the console. You can save them by running:
```bash
echo "exportparams Games/tuned_params.json" | ./build/IronRook.exe
```
