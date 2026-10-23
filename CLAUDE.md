# DOOM

id Software's 1997 linuxdoom-1.10 release, ported to 64-bit Linux on SDL3, and on its way to Godot: the original
simulation in C, presented by Godot as a modern 3D game with custom textures and models. The plan is in
`docs/godot.md`.

## Goals
- A Godot front end (lighting, materials, models, custom content) over the unchanged 1997 simulation, loaded as a
  GDExtension.
- Stay vanilla compatible: demos replay tic for tic and savegames keep the DOS v1.9 layout, in both builds.
- Keep the SDL3 build working: it is the reference the Godot build is compared against.

## Non-goals
- Changing gameplay. Improvements go to drawing, sound, input and the platform layer, not the simulation.
- A custom OpenGL renderer, or rewriting the game logic in GDScript.

## Layout
- C17 (gnu17), built with CMake (`CMakeLists.txt`), linking SDL3 and libm.
- `linuxdoom-1.10/`: the engine. Prefixes as in the original: `d_` main loop and networking, `g_` game flow, `p_`
  play simulation, `r_` renderer, `v_`/`f_`/`wi_`/`st_`/`hu_`/`am_`/`m_` 2D, screens and menus, `s_`/`i_sound`/
  `i_oplmus` sound and music, `i_` platform layer, `w_` WADs, `z_` zone allocator.
- `third_party/nuked-opl3/`: the OPL3 emulator, vendored unmodified (LGPL 2.1+).
- `ipx/`, `sersrc/`, `sndserv/`: the original DOS network drivers and Linux sound server. Not built.

## Commands
- `cmake -S . -B build && cmake --build build`: builds the project. There are no automated tests.
- `./build/linuxdoom`: runs it. It finds the IWAD in `$DOOMWADDIR`, `$DOOMWADPATH`, the current and data
  directories, the usual `games/doom` and `doom` share directories, and Steam libraries (`d_iwad.c`); `-iwad <file>`
  names one.

## Verify
Before saying a change is done, run the check and show what it printed. Never report a result you didn't see. If it
fails for a reason unrelated to the change, say so and show the output instead of working around it.
- For a change near game logic: play the demos with `-playdemo <demo> -hashlog <file>` (add `-timedemo` to run
  unthrottled) on the base and the changed build, and show that the logs match (`d_statehash.c`).
- For a visible change: run with `SDL_VIDEO_DRIVER=offscreen`, `-shotat <tic,...> -shotdir <dir>` and, to drive the
  menus, `-inputscript <file>` (`<tic> <down|up|press> <key>` per line), then look at the PNGs.

## Conventions
- Keep the original code style in `linuxdoom-1.10/`: tabs, a space before the call parenthesis, `//` comments.
- Game state changes only on tics (35 a second). Frames between tics interpolate; they never change simulation.
- Command-line overrides last only for the session and are never written to the config (`i_video.c`).

## Git
- Work on a branch named `<type>/<short-name>` (feat, fix, refactor, docs or chore), in this checkout:
  `git switch -c <type>/<short-name>`. Never commit to the default branch.
- Changes reach the default branch through a reviewed pull request: `/solo-review:review`, then `/solo-review:ship`.

## Gotchas
- Config is under `$XDG_CONFIG_HOME/linuxdoom`, savegames and screenshots under `$XDG_DATA_HOME/linuxdoom`
  (`i_system.c`).
- `-fwrapv -fno-strict-aliasing` are load-bearing: without them the optimizer can desync demos.

## More
- README.TXT (the 1997 release notes), REVIEW.md, and the commit messages, which explain each modernization step.
