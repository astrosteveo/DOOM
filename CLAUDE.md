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
- `linuxdoom-1.10/d_host.h`: the engine as a library for a host that runs it a tic at a time and reads its state
  (plain C, so C++ can include it; the engine's own headers don't compile as C++).
- `godot/`: the Godot 4.7 project. `extension/` is the GDExtension (C++, godot-cpp fetched by CMake) with the
  `DoomSim` node; `main.tscn` runs it, `level/` builds the level from what it reads and draws the things with Doom's
  sprites (`thing_sprites.gd`), `input/` posts the keyboard and mouse to the game, `screen/` shows Doom's own 2D screen
  over the 3D view and `sound/` plays its sounds and music; `tools/run_demo.gd` plays a demo headless.
  `content/definitions.cfg` lists what replaces Doom's textures, flats and sprites: easel's materials and props in
  `assets/`, lit by Doom's light (`content/doom_content.gd`).
- `art/`: easel's specs, lockfile and picks for the game's art (`~/Projects/rename-after-deciding`); `easel build`
  in `art/` writes `godot/assets/`.
- `third_party/nuked-opl3/`: the OPL3 emulator, vendored unmodified (LGPL 2.1+).
- `ipx/`, `sersrc/`, `sndserv/`: the original DOS network drivers and Linux sound server. Not built.

## Commands
- `cmake -S . -B build && cmake --build build`: builds the project. There are no automated tests.
- `./build/linuxdoom`: runs it. It finds the IWAD in `$DOOMWADDIR`, `$DOOMWADPATH`, the current and data
  directories, the usual `games/doom` and `doom` share directories, and Steam libraries (`d_iwad.c`); `-iwad <file>`
  names one.
- `cmake -S . -B build-godot -DDOOM_GODOT=ON && cmake --build build-godot --target doomgodot`: builds the extension
  into `godot/bin/` (the first configure fetches godot-cpp). Then `godot --path godot -- <doom args>` runs it; with
  no args the title loop plays the demos.

## Verify
Before saying a change is done, run the check and show what it printed. Never report a result you didn't see. If it
fails for a reason unrelated to the change, say so and show the output instead of working around it.
- For a change near game logic: play the demos with `-playdemo <demo> -hashlog <file>` (add `-timedemo` to run
  unthrottled) on the base and the changed build, and show that the logs match (`d_statehash.c`).
- For a change to the Godot project's scripts: `godot --path godot` for a few seconds, and show that it printed no
  `SCRIPT ERROR`. `tools/run_demo.gd` doesn't load `main.tscn`, and gdh imports the project first, so neither
  catches a script that only compiles once the editor has rebuilt `.godot/`.
- For a change to the host API or the extension: the same, through Godot:
  `godot --headless --path godot -s res://tools/run_demo.gd -- -playdemo demo1 -hashlog <file>`, against the SDL3
  build's log.
- For a change to what Godot shows: `gdh live start --project godot -- -iwad <path> -playdemo demo1`, step, and look;
  `-shotat` on the SDL3 build renders the same tic to compare. Add `-classic` to leave out Godot's lights, fog and
  glow, which the SDL3 build doesn't have.
- For a change to sound: `gdh movie --project godot --out <dir> --seconds 25 -- -iwad <path>` records the title loop
  with its sound; read `audio.png`. `-classic` plays the SDL3 build's own mix, to compare. `gdh live audio` measures
  only what was mixed in real time, and a held game's players read as stopped.
- For a visible change: run with `SDL_VIDEO_DRIVER=offscreen`, `-shotat <tic,...> -shotdir <dir>` (make the
  directory first: shots to a missing one are lost without a word) and, to drive the
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
- No `class_name` in `godot/`: a script that uses another `preload`s it by path into a constant of the same name.
  Godot finds `class_name`s through `.godot/`'s class cache, which is gitignored and only the editor rebuilds, so a
  checkout whose cache predates a new class fails to compile the scripts that use it.
- Under gdh, `$XDG_DATA_HOME` is gdh's own, so the game doesn't find the IWAD: pass `-iwad`.
- `-record <name>` takes at most 27 characters (`demoname` is 32 bytes with `.lmp`): a longer path overflows it and
  crashes in both builds. Opening the menu while recording desyncs the demo, as in vanilla.
- Only one game runs per process (the engine is globals), and `I_Quit`/`I_Error` jump back to `DH_Start`/`DH_Tic`
  through `I_HostReturn` rather than exiting.
- The level is built from the WAD at run time, so `main.tscn` shows only the camera in the editor.

## More
- README.TXT (the 1997 release notes), REVIEW.md, and the commit messages, which explain each modernization step.
