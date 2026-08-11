# GG simulator SD card

The PC simulator treats this directory as the optional SD card.

- `GG/images`: Gallery files (`.jpg`, `.jpeg`, `.png`, `.bmp`, `.gif`, `.bin`)
- `GG/music`: Music files (`.wav`, `.mp3`, `.flac`, `.ogg`; WAV playback implemented)
- `GG/games`: original Game Boy DMG ROMs (`.gb`) and runtime save files (`.sav`)

Game only lists ROMs that pass the shared header, checksum, size, DMG compatibility,
and cartridge-controller checks. It runs them with the vendored MIT Peanut-GB core;
Game Boy audio is currently silent. Use only ROMs you own and are allowed to use.

Set `GG_SD_ROOT` to use another directory.
