# DTACKSpeedTest

A Mac Portable memory speed diagnostic tool that measures read timing across the expansion RAM address range and compares it to built-in RAM.

## Background

The Mac Portable's GLU chip controls DTACK (bus cycle completion) timing for all memory regions. Expansion RAM lives at `$100000–$8FFFFF`. Different sub-ranges within that window may have different DTACK timing characteristics, and timing can change after sleep/wake cycles.

This tool was developed while investigating a speed regression observed with 8MB expansion cards (9MB total system RAM) on the Mac Portable. Snooper's Memory Move benchmark showed ~51% of expected speed with 8MB vs ~85% with 4MB. DTACKSpeedTest was built to isolate whether the issue was DTACK read timing and, if so, at which specific address range it fell off.

**Finding:** Read DTACK timing is stable throughout the full expansion range. The lower 4MB (`$100000–$4FFFFF`) runs at ~1.00x and the upper 4MB (`$500000–$8FFFFF`) at ~1.03x — essentially unchanged. The Snooper slowdown appears to originate in write cycle timing, not reads.

## What it does

- Detects installed RAM via Gestalt (supports 4MB and 8MB expansion cards)
- Times sequential reads at each 1MB boundary across the expansion range
- Reports a ratio vs built-in RAM baseline (`$004000`)
- Tests the A22 boundary specifically (`$3FC000` and `$404000`)
- Saves results to `DTACKResults.txt` alongside the application
- Prompts for a sleep/wake cycle so you can compare before and after

## Requirements

- Mac Portable (M5120 or M5126) with a 4MB or 8MB RAM expansion card
- System 6 or System 7

## Running it

1. Decode `DTACKSpeedTest.hqx` with StuffIt Expander
2. Copy `DTACKSpeedTest` to your Mac Portable (via AFP, floppy, or BlueSCSI)
3. Launch the application — results appear on screen and are logged to `DTACKResults.txt`
4. To test sleep/wake: note the Run 1 ratios, put the machine to sleep, wake it, then press any key for Run 2

## Interpreting results

| Ratio | Label | Meaning |
|-------|-------|---------|
| < 1.30x | FAST | Timing unchanged from baseline |
| 1.30–2.00x | MARGINAL | Slightly degraded |
| > 2.00x | SLOW | Significant DTACK slowdown |

## Building from source

Requires [Retro68](https://github.com/autc04/Retro68) toolchain.

```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/Retro68/toolchain/m68k-apple-macos.cmake
make
```

The build produces `DTACKSpeedTest.APPL` and `DTACKSpeedTest.dsk`.

## Limitations

- Tests **read cycles only** — write timing is not measured
- Requires exactly 5MB (4MB card) or 9MB (8MB card) total RAM; other configurations are rejected

## Related projects

- [MacintoshPortable8MB_RAM](https://github.com/2tf2294x4g-maker/MacintoshPortable8MB_RAM) — 8MB RAM expansion card for the Mac Portable
- [MacintoshPortable4MB_RAM](https://github.com/2tf2294x4g-maker/MacintoshPortable4MB_RAM) — 4MB RAM expansion card for the Mac Portable

## License

[MIT](LICENSE) · SPDX `MIT` — Copyright (c) 2026 Greg Campbell.
The full licence text is included in this repository.
