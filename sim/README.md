# Pedal Display PC Simulator

This builds the same UI/app sources against LVGL 9.5 and SDL2 so launcher,
theme, and app behavior can be checked on a 480x320 desktop window.

## Build

```powershell
cmake -S sim -B sim/build -G Ninja
cmake --build sim/build
.\sim\build\pedal_sim.exe
```

If SDL2 is installed in a non-standard location, pass `-DSDL2_DIR=...` or set
`CMAKE_PREFIX_PATH` to the SDL2 package prefix before configuring.

On Windows, the command prompt also needs a host C/C++ compiler in `PATH`
(Visual Studio Build Tools, MSYS2/MinGW, or equivalent). If `cmake` is not in
`PATH`, use the full path to an installed CMake executable.

## Controls

- Arrow keys: `EV_UP`, `EV_DOWN`, `EV_LEFT`, `EV_RIGHT`
- Enter: `EV_OK`
- Backspace: `EV_HOME`; hold for 500 ms sends `EV_HOME_HOLD`
- Space: `EV_FOOTSW`; hold for 500 ms sends `EV_FOOTSW_HOLD`
- `O`: injects a synthetic music onset when audio capture is unavailable
- Mouse X: synthetic pitch from E2 to E5 when audio capture is unavailable
- Escape: quit

## Audio Input

On Windows, the simulator uses WASAPI loopback by default. Audio currently
playing through the Windows default speakers or headphones is downmixed and
resampled to the firmware's 48 kHz mono analysis input, then fed to the shared
visualizer, tuner, dB meter, and music-event DSP code.

The simulator compiles the firmware's `fft_map.c` directly. Its 2048-point
window, 23.4375 Hz FFT bins, logarithmic mapping, averaging, release, and peak
hold therefore match the gadget. Only audio capture (WASAPI/SDL versus I2S) and
the platform FFT executor (portable C versus ESP-DSP) differ.

```powershell
.\sim\build\pedal_sim.exe
```

`--system-audio` selects the same path explicitly. To use the default
microphone or audio-interface capture input instead:

```powershell
.\sim\build\pedal_sim.exe --microphone
```

To list the system-audio path and SDL2 capture inputs:

```powershell
.\sim\build\pedal_sim.exe --list-audio
```

To select a device by index:

```powershell
.\sim\build\pedal_sim.exe --audio-device 1
```

`--audio-device N` implies capture-device mode. On non-Windows platforms the
default remains the SDL2 capture device. If neither Windows loopback nor a
capture device can be opened, the simulator prints a warning and keeps the
synthetic visualizer, mouse pitch, and `O` onset path.

To preview the visualizer deterministically without opening a capture device:

```powershell
.\sim\build\pedal_sim.exe --synthetic
```

To open a deterministic visual QA screen without NVS state or key input:

```powershell
.\sim\build\pedal_sim.exe --preview curve
.\sim\build\pedal_sim.exe --preview reference
.\sim\build\pedal_sim.exe --preview bars
.\sim\build\pedal_sim.exe --preview circular
.\sim\build\pedal_sim.exe --preview dbmeter
.\sim\build\pedal_sim.exe --preview bounce
.\sim\build\pedal_sim.exe --preview monitor-settings
.\sim\build\pedal_sim.exe --preview monitor-color
.\sim\build\pedal_sim.exe --preview monitor-mode
```

Persistent launcher/theme/slot state is written to `sim_nvs.bin` in the current
working directory.

## Deterministic Smoke Test

Run the launcher, app dispatch, synthetic visualizer, shared tuner DSP, Bounce
jump/collision/restart cycle, quick app, and app cleanup checks without keyboard
automation:

```powershell
.\sim\build\pedal_sim.exe --smoke-test
```

This mode forces synthetic audio, starts from default launcher state, and does
not read or write `sim_nvs.bin`. It exits with code 0 and prints `SMOKE PASS`
when every check succeeds.
