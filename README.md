# DSTweaks — source

Source code of the installer/patcher used by the **DSTweaks** mod for
*Death Stranding Director's Cut*. Published for transparency and for the Nexus
Mods manual review process.

The tool edits two data files inside the game's own archive (weapon damage /
range / magazine size) and repacks the archive locally. It does not inject into
the game process, does not use a loader, does not touch save games and does not
access the network.

## What is in here

| File | Purpose |
| --- | --- |
| `launcher.c` | console front-end: game detection, backup, install/restore menu, calls the patchers, drives the repack, restores on failure |
| `dspatch.c` | verifies and patches `attack_parameter_collection.core` (damage / range) |
| `dspatch_mag.c` | verifies and patches `magazine_parameter_collection.core` (magazine / pellets) |
| `dspatch_zip.c` | verifies and patches `dsplayerparamcomponent.core` (zipline speed / turn / acceleration) |
| `dsconfig.c/.h` | `DSDamage.ini` parser shared by both patchers |
| `vanilla_table.h` | 247 vanilla attack records (id + damage bits) used for verification |
| `attack_categories.h` | category per `EDSAttackId` |
| `enemy_table.h` | attack ids that belong to enemy / NPC / story weapons |
| `mag_table.h` | 134 vanilla magazine records (id, category, enemy flag, field offsets, clip, pellets, scale bits) |

## Build

Zig (what the released binaries were built with):

```sh
zig cc -target x86_64-windows-gnu -O2 -Dmain=dd_attack_main -c dspatch.c -o dspatch.o
zig cc -target x86_64-windows-gnu -O2 -Dmain=dd_mag_main    -c dspatch_mag.c -o dspatch_mag.o
zig cc -target x86_64-windows-gnu -O2 -c dsconfig.c -o dsconfig.o
zig cc -target x86_64-windows-gnu -O2 -static -o DSTweaks.exe \
       launcher.c dspatch.o dspatch_mag.o dsconfig.o
```

MSVC:

```bat
cl /O2 /Dmain=dd_attack_main /c dspatch.c
cl /O2 /Dmain=dd_mag_main /c dspatch_mag.c
cl /O2 /c dsconfig.c
cl /O2 launcher.c dspatch.obj dspatch_mag.obj dsconfig.obj /Fe:DSTweaks.exe
```

`DSTweaks.exe` expects the release package layout (`bin\DecimaExplorer.exe`,
`DSDamage.ini`, and the game's `oo2core_7_win64.dll` copied at runtime).

## Third-party

`bin\DecimaExplorer.exe` in the release package is
[Decima-Explorer](https://github.com/Jayveer/Decima-Explorer) by Jayveer (MIT).
It is not part of this repository. The Microsoft Visual C++ runtime DLLs bundled
with the release are required by it.

## License

MIT — see `LICENSE`.
