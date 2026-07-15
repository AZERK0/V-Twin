# V-Twin

![V-Twin screenshot](docs/public/screenshots/screenshot_v01.png?raw=true)

V-Twin is a fork of Engine Simulator focused on real-time engine analysis, audio, thermal observation, and diagnostics. Its dedicated engine-condition screen separates physical simulation outputs from metrics that still require calibration.

## What V-Twin is for

- simulating internal combustion engines in real time
- producing engine audio and response behavior
- exploring operating conditions and thermal behavior
- building UI and tooling around engine-condition visualization

V-Twin is still a simulator, not a certified engineering tool. Oil, piston, and cylinder values are lumped thermal estimates rather than local sensor measurements.

## Engine condition screen

The engine-condition screen is a full-screen engineering view, separate from the engine and exhaust-flow analysis screen.

It introduces:

- a published snapshot that keeps simulation data out of rendering code
- large gauges for oil, hottest-piston, and hottest-cylinder temperatures
- a per-cylinder thermal map
- the same interactive 2D engine view used by the analysis screens
- compact layouts of the existing shared telemetry components
- direct engine, vehicle, transmission, dyno, intake, and combustion telemetry
- explicit model coverage and data-quality information

Key metrics exposed by the screen include:

- simulated oil, hottest-piston, and hottest-cylinder temperatures
- engine and vehicle speed, gear, torque, and power
- manifold pressure, air flow, and volumetric efficiency
- ignition, starter, dyno, and hold states
- throttle, intake AFR, exhaust oxygen, and thermal-input validity

See `ENGINE_WEAR_METRICS.md` for the displayed metrics and `ENGINE_THERMAL_MODEL.md` for the equations, implementation mapping, assumptions, and current physical limits.

## Controls

The simulator keeps the original keyboard-driven workflow, with a shortcut for the engine-condition screen.

| Key/Input | Action |
| :---: | :--- |
| `A` | Toggle ignition |
| `S` | Hold for starter |
| `D` | Enable dyno |
| `H` | Enable RPM hold |
| `G` + scroll | Change hold speed |
| `F` | Toggle fullscreen |
| `I` | Display dyno stats in the information panel |
| `J` | Switch between engine analysis and engine condition |
| `Shift` | Clutch |
| `Space` + scroll | Fine throttle adjustment |
| `Up Arrow` | Up gear |
| `Down Arrow` | Down gear |
| `Q`, `W`, `E`, `R` | Change throttle position |
| `Z` + scroll | Volume |
| `X` + scroll | Convolution level |
| `C` + scroll | High-frequency gain |
| `V` + scroll | Low-frequency noise |
| `B` + scroll | High-frequency noise |
| `N` + scroll | Simulation frequency |
| `M` | Increase view layer |
| `,` | Decrease view layer |
| `Enter` | Reload engine script |
| `Tab` | Change screen |
| `Escape` | Exit |

### RPM hold

Press `H` to arm RPM hold, then enable the dyno with `D`. While hold is active, use `G` + scroll to change the target speed.

## Linux build

This fork includes a Linux workflow with helper scripts, a `Makefile`, and CMake presets.

### Requirements

Install the following with your distribution package manager:

- `git`
- `cmake`
- `pkg-config`
- `boost`
- `flex`
- `bison`
- `SDL2`
- `SDL2_image`

### Quickstart

From the repository root:

```bash
make bootstrap
make configure
make build
make run
```

### Equivalent script-based workflow

```bash
./scripts/bootstrap-linux.sh
./scripts/configure.sh linux-release
./scripts/build.sh build-release
./scripts/run.sh
```

### Useful commands

```bash
make build-debug
make test
```

Or directly with presets:

```bash
cmake --preset linux-release
cmake --build --preset build-release
ctest --preset test-release
```

### Build outputs

- release binary: `build/linux-release/engine-sim-app`
- debug binary: `build/linux-debug/engine-sim-app`

The Linux scripts also prepare the runtime `delta.conf` files and run the simulator with `ENGINE_SIM_DATA_ROOT` set correctly.

## Python launcher GUI

An optional Python launcher GUI can be used to select an engine and start the simulator more easily when `launcher_python/` is available in your checkout.

### Run the launcher

```bash
python3 launcher_python/main.py
```

The launcher expects the simulator binary at `build/linux-release/engine-sim-app`, so build the project first.

Typical workflow:

```bash
make build
python3 launcher_python/main.py
```

### Recommended Python setup

Using a local virtual environment keeps the Python tooling isolated:

```bash
python -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python launcher_python/main.py
```

### Tk dependency on Linux

The GUI uses `tkinter`, which depends on Tk being installed on the system.

On Arch Linux:

```bash
sudo pacman -S tk
```

You can verify the GUI stack with:

```bash
python3 -m tkinter
```

## Project structure

- `src/` and `include/`: core simulator, rendering, and UI code
- `src/simulation/engine_condition_model.cpp`: condition snapshot implementation
- `src/ui/engine_condition_cluster.cpp`: dedicated condition-screen rendering
- `ENGINE_WEAR_METRICS.md`: displayed metrics, formulas, provenance, and wear-model limits
- `ENGINE_THERMAL_MODEL.md`: oil, piston, and cylinder thermal-model specification and implementation
- `assets/engines/`: sample engine definitions
- `launcher_python/`: optional Python launcher GUI
- `scripts/`: Linux bootstrap, configure, build, test, and run helpers

## Notes

- this repository is actively evolving
- some historical upstream references may still exist in code or asset names
- the current fork direction prioritizes V-Twin branding, Linux usability, and trustworthy engine diagnostics
