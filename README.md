# Ultimate Spider-Man (Nintendo Switch Native Port)

A native Nintendo Switch port of **Ultimate Spider-Man (PC, 2005)**, built by statically recompiling the original PC binary (`USM.exe`) directly into optimized C code (translating every x86 instruction to portable, equivalent C, similar to N64Recomp and XenonRecomp), coupled with custom Direct3D 9, DirectInput, DirectSound, and Win32 runtime emulation layers targeting modern OpenGL / EGL and Nintendo Horizon OS (`libnx`).

> [!NOTE]
> **AI-Assisted Port**: This Nintendo Switch port, including its memory virtualization architecture, OpenGL 4.3 core translation pipeline, crash diagnostics, native Joy-Con input subsystem, and SwitchWaker-inspired overlay menu, was developed with the assistance of **Google DeepMind's Antigravity AI agent**.

---

## Performance & Resolution Profiles

- **720p Handheld (1280x720)**: Runs at a solid, locked **30 FPS** at stock Nintendo Switch clocks (1020 MHz CPU / 307 MHz GPU). Delivers crisp 1:1 native pixel mapping on the Switch internal display with zero blurry scaling.
- **1080p Docked / Enhanced (1920x1080)**: Native full HD 1080p rendering. **Needs overclocking** (e.g., Boost/High CPU clock and High GPU profile via the in-game overlay menu) to achieve a stable **30 FPS**.
- **Changing Resolutions**: To change internal rendering resolution:
  1. Open the in-game overlay menu by pressing **Minus (`-`)** (or **L + R + Plus**).
  2. In the **Graphics** tab, select your preferred resolution (720p Native, 1080p Docked, 544p, or 480p).
  3. Navigate to the **System** tab and select **Save settings to SD** (`[ Press A to Save ]`).
  4. **Restart the game** for the new resolution to take effect.
- **Frame Generation (Experimental 40 FPS Boost)**: An experimental frame generation mode that synthesizes 10 intermediate frames per second (1 generated frame every 3 real frames) to boost presentation from 30 FPS to 40 FPS.
  > [!WARNING]
  > Frame generation is strictly experimental and does not work smoothly just yet. For the best and smoothest gameplay experience, leave Frame Generation **Off** and play at native 30 FPS.

---

## Features

- **Native Horizon OS Execution**: Runs as a standard Homebrew NRO application (`usm_switch.nro`) directly on Nintendo Switch hardware.
- **High-Definition Rendering (720p / 1080p)**: Native 1280x720 internal rendering for crisp handheld play, with optional 1920x1080 mode for docked play.
- **Framerate Targets & Overclocking Profiles**: Configurable frame rate caps (20 / 30 / 40 FPS) with integrated official Nintendo GPU performance profiles (Stock 307 MHz / Boost 384 MHz / High 460 MHz) and CPU clocks (Stock 1020 MHz / Boost 1224 MHz / High 1581 MHz / Max 1785 MHz via `clkrst`), automatically restored to stock on exit.
- **SwitchWaker In-Game Overlay Menu**: Accessible at any moment during gameplay by pressing **Minus (`-`)** or **L + R + Plus**:
  - **Graphics**: View Distance, Manhattan Fog, Particle Detail, Resolution Mode (720p Crisp / 1080p Docked / 544p / 480p).
  - **Controls**: Camera Invert X/Y, Stick Sensitivity, HD Rumble, Swap A/B buttons, plus a complete read-only controller button mapping table.
  - **Performance**: Frame rate cap (20 / 30 / 40 FPS), Frame Generation toggle (Experimental), VSync, On-Screen FPS Counter, GPU Performance Profile, CPU Overclock, Traffic & Pedestrian Density.
  - **QoL**: Skip intro movies, Fast map pause.
  - **System**: Save settings to SD (`settings.ini` & `resolution.txt`), Restore player defaults.
  - **Debug**: Wireframe mode, Session logging (`log.txt`), Available Horizon RAM monitor.
- **Native Joy-Con & Pro Controller Mapping**: Standard Xbox controller button layout with dedicated buttons for Jump, Web Zip, Web Swing, Attacks, Pause, and Map on Left Stick Click.
- **Clean In-Game Exit**: Selecting "Quit" from the in-game main menu cleanly restores hardware clocks and exits the application back to Horizon OS / Home menu without freezing.
- **Full Video & Audio**: Integrated Bink video cutscenes (via FFmpeg) and full 3D positional DirectSound streaming.

---

## Requirements

### To Play on Nintendo Switch:
1. A Nintendo Switch console running Atmosphere custom firmware with Homebrew Launcher access (or HOME-menu forwarder).
2. Your legally owned PC install files of *Ultimate Spider-Man (2005)*:
   - `USM.exe` (SHA-256: `5d554ef69e8066eed9e327a4fe331992124aee6de16af54bd7ac5c5dcb0488b8`).
   - The entire `Data/` folder (including `Data/movies`, `Data/Sound`, and `Data/packs`).

### To Build the Port from Source:
- Linux or Windows Subsystem for Linux (WSL2, Ubuntu recommended).
- **Docker** (recommended — automatically pulls the official `devkitpro/devkita64` toolchain) OR a local installation of **DevkitPro** (`devkitA64`).
- **Python 3** with `capstone`:
  ```bash
  pip install capstone
  ```
- `cmake`, `ninja`, and `git`.

---

## Step-by-Step Porting & Build Guide

Follow these exact steps to compile your own working Nintendo Switch NRO from scratch:

### Step 1: Clone the Repository
```bash
git clone https://github.com/your-repo/usm_switch.git
cd usm_switch
```

### Step 2: Static Recompilation of `USM.exe`
Place your legally obtained PC `USM.exe` into the repository root:
```bash
# 1. Verify the SHA-256 hash of your executable
sha256sum USM.exe
# Expected output: 5d554ef69e8066eed9e327a4fe331992124aee6de16af54bd7ac5c5dcb0488b8

# 2. Run the static recompiler to translate x86 instructions to C:
python3 tools/recomp/recomp.py USM.exe data/cfg.txt src/recomp/gen/
```
*Note: This process analyzes the control flow graph and generates ~230 MB of translated C files in `src/recomp/gen/`.*

### Step 3: Compile the Nintendo Switch NRO
Run the automated build script. If Docker is installed, it will automatically pull and run inside the official `devkitpro/devkita64` container:
```bash
chmod +x build_switch.sh
./build_switch.sh
```

If building on a machine with a local devkitPro installation, ensure `$DEVKITPRO` and `$DEVKITA64` are exported in your environment:
```bash
export DEVKITPRO=/opt/devkitpro
export DEVKITA64=/opt/devkitpro/devkitA64
export PATH=$DEVKITA64/bin:$DEVKITPRO/tools/bin:$PATH

cmake -S switch -B build-switch -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake \
  -DCMAKE_BUILD_TYPE=Release
ninja -C build-switch -j$(nproc)
```

Upon a successful build, the final executable will be produced at:
```
build-switch/usm_switch.nro
```

---

## SD Card Installation

1. Copy `usm_switch.nro` to your Nintendo Switch SD card:
   - Location: `sdmc:/switch/usm/usm_switch.nro`
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
3. Launch `usm_switch` from the Homebrew Launcher.
   > [!IMPORTANT]
   > For the best performance and stability, launch via **title-override mode** (hold `R` while launching any installed Switch game from the HOME menu) so that the Homebrew Launcher grants the port full RAM access (~3.1 GB available memory).

---

## Controls

The port utilizes standard modern Xbox controller layout:

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
| **Minus (`-`)** or **L+R+Plus** | — | **SwitchWaker Overlay Menu** (Settings & Performance) |

---

## Technical Details

- **Direct3D 9 to OpenGL 4.3 Core**: D3D9 state, shaders, textures, and vertex/index buffers are translated on the fly to modern OpenGL 4.3 Core via EGL and libnx.
- **Memory Virtualization**: 32-bit x86 pointer operations are mapped into Horizon OS 64-bit virtual memory space using native `svcQueryMemory` memory boundary checks.
- **Hardware Clocks & Performance Profiles**: Uses official Horizon OS `apm` (`apmSetPerformanceConfiguration`) and `clkrst` (`clkrstSetClockRate`) APIs to adjust handheld CPU/GPU frequencies safely.

---

## Credits & Acknowledgments

- **usm_vita**: Heartfelt thanks and credit to the **usm_vita** project and its authors. Without their groundbreaking work on the x86-to-C static recompilation pipeline, control-flow graph analysis, D3D-to-GL translation research, and Win32/DirectX runtime shims, this Nintendo Switch port would not have even started off the ground.
- **SwitchWaker**: Sincere gratitude to the **SwitchWaker** (*The Legend of Zelda: The Wind Waker* Switch decompilation) project and team. Their in-game overlay architecture, options menu design, clkrst / apm hardware clock tuning implementations, and performance profiling methodology directly inspired and formed the foundation for this port's in-game overlay and hardware controls.
- **Treyarch, Activision & Marvel** for *Ultimate Spider-Man* (2005).
- **Rinnegatamante** for vitaGL and initial recompilation research.
- **Google DeepMind Antigravity AI**: Developed and engineered the Nintendo Switch port, including crash diagnostics, OpenGL shader pipeline, Horizon memory virtualization, input mapping, and SwitchWaker-inspired UI and overclocking implementations.
