# Changelog

What changed in each release, written for someone deciding whether to grab a new
build. `scripts/release_notes.sh` reads the section for a version out of this file
and the Release workflow publishes it as the release body, so the notes on the
release page and the notes here cannot drift apart.

Adding a version here is part of cutting a release: the workflow fails the tag if
the section is missing. The heading must be exactly `## vMAJOR.MINOR.PATCH`.

Versions before v0.7.0 were backfilled from the commits and the release pages
after the fact; this file is the record from here on. Releases before v0.6.0
(v0.2.0 through v0.5.1) predate the split into the standalone `mach.h` engine and
are left in the git tags only.

## v0.7.0

The game grows a spine. There is now a reason to keep playing, a way to keep a
game, and menus to move between them.

**A money economy.** Money is a balance you spend, not just a score. The build
area starts small (you cannot even fit a loop) and is a centered square you pay to
double, on a cost that scales with the space unlocked. Droppers and upgraders come
in tiers: a higher-tier dropper emits richer ore, a higher-tier upgrader lifts an
ore's ceiling harder, and both cost more. A right-side shop panel sells tools and
tiers and shows the prices; clicking a button never also drops a tile in the world
behind it.

**Save and load.** One slot, written to disk, that restores the whole game: the
build region, every machine and its tier, ore in flight, money, and the camera. K
saves, L loads in-game; the title screen offers Continue when a save exists.

**A title screen and a pause menu.** The game launches to a menu (New Game,
Continue, Quit). Escape in-game now pauses and opens an overlay to resume or return
to the title, instead of closing the window.

**An animation layer.** The simulation no longer times its own visuals. It records
events (an ore banked, an ore tipped off a dead end) and the renderer plays them on
real time, so effects stay smooth and independent of the fixed tick. Banking is
visible now: the ore drops into the furnace and a "+value" rises, and two payouts
at once no longer stack their text on one spot.

**The furnace, reshaped and renamed.** The old "collector" is a furnace throughout,
and it reads as a shallow open bin rather than a tall block, so ore drops into it
instead of colliding with a tower.

**Real sprite art, baked in.** Drop a PNG into `assets/sprites/` and it is compiled
into the executable; a missing sprite falls back to the procedural block. The
shipped binary is still a single self-contained file per platform.

**Smaller fixes.** Frames pace to vsync (no tearing, no busy-wait), and the F3
overlay shows real frame time. Ore is visible on the first conveyor in short
layouts. World-space value labels scale with zoom instead of ballooning when zoomed
out. `./nob hot` stopped spamming a directory-exists line every half second.

## v0.6.2

Automated releases. Tagging `vX.Y.Z` now builds the game on Linux, macOS, and
Windows and attaches a static, self-contained binary for each (macOS universal,
Linux on an old-enough glibc, Windows with no redistributable needed). The Linux
CI build also gained the `libxi`/`libxext` dev headers it was missing.

## v0.6.1

The build tool is [nob](https://github.com/tsoding/nob.h) now: compile `nob.c`
once, then `./nob`. `build.sh` and `build.bat` are gone. Added a README warning
that this is a personal, actively-developed project with no stability promise.

## v0.6.0

The engine moved out into its own single-header project, `mach.h`, and this repo
vendors a copy at `src/mach.h`, so the game still builds with nothing but a C
compiler. The game relicensed to PolyForm Noncommercial 1.0.0 (the engine stays
zlib). Docs were restructured, and the debug overlay moved to the backtick key.
