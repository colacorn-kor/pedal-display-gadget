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
- `H`: `EV_HOME`; hold for 500 ms sends `EV_HOME_HOLD`
- `F`: `EV_FOOTSW`; hold for 500 ms sends `EV_FOOTSW_HOLD`
- Space: injects a synthetic music onset for Bounce
- Mouse X: synthetic pitch from E2 to E5
- `Q` or Escape: quit

Persistent launcher/theme/slot state is written to `sim_nvs.bin` in the current
working directory.
