# Ultimate Spider-Man (Nintendo Switch Native Port)

A native Nintendo Switch port of **Ultimate Spider-Man (PC, 2005)**, built by statically recompiling the original PC binary (`USM.exe`) directly into optimized C code (translating every x86 instruction to portable, equivalent C, similar to N64Recomp and XenonRecomp), coupled with custom Direct3D 9, DirectInput, DirectSound, and Win32 runtime emulation layers targeting modern OpenGL / EGL and Nintendo Horizon OS (`libnx`).

> [!NOTE]
> **AI-Assisted Port**: This Nintendo Switch port, including its memory virtualization architecture, OpenGL 4.3 core translation pipeline, crash diagnostics, native Joy-Con input subsystem, and SwitchWaker-inspired overlay menu, was developed with the assistance of **Google DeepMind's Antigravity AI agent**.

---

## Features

- **Native Horizon OS Execution**: Runs as a standard Homebrew NRO application (`usm_switch.nro`) directly on Nintendo Switch hardware.
- **High-Definition Rendering (720p / 1080p)**: Native 1280x720 internal rendering for crisp 1:1 handheld play, with optional 1920x1080 mode for docked play. Eliminates pixelated, blurry legacy scaling.
- **Target 60 FPS & Overclocking Profiles**: Configurable frame rate caps (20 / 30 / 60 FPS) with integrated official Nintendo GPU performance profiles (up to 460 MHz) and CPU clocks (up to 1785 MHz via `clkrst`), automatically restored on exit.
- **SwitchWaker In-Game Overlay Menu**: Accessible at any moment during gameplay by pressing **Minus (`-`)**:
  - **Graphics**: View Distance, Manhattan Fog, Particle Detail, Resolution Mode (720p Crisp / 1080p Docked / 544p / 480p).
  - **Controls**: Camera Invert X/Y, Stick Sensitivity, HD Rumble, Swap A/B buttons, plus a complete read-only controller button mapping table.
  - **Performance**: Frame rate cap (20/30/60 FPS), VSync, On-Screen FPS Counter, GPU Performance Profile (307 / 384 / 460 MHz), CPU Overclock (1020 / 1224 / 1581 / 1785 MHz), Traffic & Pedestrian Density.
  - **QoL**: Skip intro movies, Fast map pause.
  - **System**: Save settings to SD (`settings.ini` & `resolution.txt`), Restore player defaults.
  - **Debug**: Wireframe mode, Session logging (`log.txt`), Available Horizon RAM monitor.
- **Native Joy-Con & Pro Controller Mapping**: Standard Xbox controller button layout with dedicated buttons for Jump, Web Zip, Web Swing, Attacks, Pause, and Map on Left Stick Click.
- **Clean In-Game Exit**: Selecting "Quit" from the in-game main menu cleanly restores hardware clocks and exits the application back to the Home menu / hbmenu without freezing.
- **Full Video & Audio**: Integrated Bink video cutscenes (via FFmpeg) and full 3D positional DirectSound streaming.

---

## Requirements

### To Play on Nintendo Switch:
1. A Nintendo Switch console running Atmosphere custom firmware with Homebrew Launcher access (or HOME-menu forwarder).
2. Your legally owned PC install files of *Ultimate Spider-Man (2005)*:
   - `USM.exe` (SHA-256: `5d554ef69e8066eed9e327a4fe331992124aee6de16af54bd7ac5c5dcb0488b8`).
   - The entire `Data/` folder (including `Data/movies`, `Data/Sound`, and `Data/packs`).

### To Build the Port from Source:
- Linux or WSL2 (Ubuntu recommended).
- **Docker** (recommended, automatically pulls `devkitpro/devkita64`) OR a local installation of **DevkitPro** (`devkitA64`).
- **Python 3** with `capstone`:
  ```bash
  pip install capstone
  ```
- `cmake` and `ninja`.

---

## Step-by-Step Porting & Build Guide

### Step 1: Clone the Repository
```bash
git clone https://github.com/your-repo/usm_switch.git
cd usm_switch
```

### Step 2: Static Recompilation of `USM.exe`
Place your legally obtained PC `USM.exe` in the project root:
```bash
# Verify the hash of your executable
sha256sum USM.exe
# Expected: 5d554ef69e8066eed9e327a4fe331992124aee6de16af54bd7ac5c5dcb0488b8

# Run the static recompiler to translate x86 instructions to C:
python3 tools/recomp/recomp.py USM.exe data/cfg.txt src/recomp/gen/
```
*Note: This process analyzes the control flow graph and generates ~230 MB of translated C files in `src/recomp/gen/`.*

### Step 3: Build the Nintendo Switch NRO
Build the project using the automated build script (which utilizes the official devkitA64 container):
```bash
chmod +x build_switch.sh
./build_switch.sh
```
Upon completion, the final binary will be generated at:
```
build-switch/usm_switch.nro
```

---

## SD Card Installation

1. Copy `usm_switch.nro` to your Nintendo Switch SD card:
   - Recommended: `sdmc:/switch/usm/usm_switch.nro` (or `sdmc:/switch/usm_switch.nro`).
2. Copy `USM.exe` and the `Data/` folder from your PC installation into `sdmc:/switch/usm/`:
   ```
   sdmc:/
   └── switch/
       └── usm/
           ├── usm_switch.nro
           ├── USM.exe
           ├── settings.ini         (optional, auto-generated on first save)
           ├── resolution.txt       (optional, defaults to 1280x720)
           └── Data/
               ├── movies/
               │   └── pc/          (*.dat Bink video cutscenes)
               ├── Sound/           (*.pck audio archives)
               ├── packs/           (*.pkg asset packs)
               └── usm_lte.usm      (English text dictionary)
   ```
3. Launch `usm_switch` from the Homebrew Launcher (hold `R` while launching any game for title-override mode with full RAM access).

---

## Controls

The port uses standard modern Xbox controller layout:

| Switch Button | Xbox Equivalent | In-Game Action |
|---|---|---|
| **B** (or **A** if swapped) | **A** | **Jump** / Release Web Swing / Confirm |
| **A** (or **B** if swapped) | **B** | **Wall Crawl** / Web Action / Sprint / Cancel |
| **Y** | **X** | **Punch** / Light Combo Attack |
| **X** | **Y** | **Kick** / Heavy Combo Attack |
| **L / ZL** | **LB / LT** | **Web Zip** |
| **R / ZR** | **RB / RT** | **Web Swing** / Shoot Web |
| **Left Stick** | Left Stick | Move Spider-Man / Navigate Menus |
| **Right Stick** | Right Stick | Rotate Camera |
| **Left Stick Click (LS)** | Back | **Open City Map** |
| **Right Stick Click (RS)** | RS Click | **Center Camera Behind Player** |
| **Plus (`+`)** | Start | **Pause Game** / In-game Menu |
| **Minus (`-`)** | — | **SwitchWaker Overlay Menu** (Settings & Performance) |

---

## Technical Details

- **Direct3D 9 to OpenGL 4.3**: D3D9 state, shaders, textures, and vertex/index buffers are translated on the fly to modern OpenGL 4.3 Core via EGL.
- **Memory Virtualization**: 32-bit x86 pointer operations are mapped into Horizon OS 64-bit virtual memory space using native `svcQueryMemory` memory boundary checks.
- **Clocks & Performance Profiles**: Uses official Horizon OS `apm` (`apmSetPerformanceConfiguration`) and `clkrst` (`clkrstSetClockRate`) APIs to adjust handheld CPU/GPU frequencies safely.

---

## Credits & Acknowledgments

- **Treyarch, Activision & Marvel** for *Ultimate Spider-Man* (2005).
- **Rinnegatamante** for vitaGL and initial recompilation research.
- **Google DeepMind Antigravity AI**: Developed and engineered the Nintendo Switch port, including crash diagnostics, OpenGL shader pipeline, Horizon memory virtualization, and SwitchWaker-inspired UI and overclocking implementations.
