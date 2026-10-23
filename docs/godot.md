# Doom on Godot

The aim is to see how beautiful and modern Doom can look: lit 3D levels, PBR materials, custom textures and models
in place of sprites. Godot presents the game; the 1997 simulation still runs it.

## Why this shape

- **Godot, not a renderer of our own.** Godot has PBR, shadows, global illumination, fog, glow, glTF import and
  animation, and an editor to light scenes in. easel (`~/Projects/rename-after-deciding`) makes textures, props
  and, later, characters for Godot. A custom OpenGL renderer would rebuild all of that first.
- **The C simulation, not a rewrite.** The game is Doom because the 1997 code runs it: demos stay in sync,
  savegames stay v1.9, and `-hashlog` proves nothing drifted. A GDScript rewrite would give up all three.
- **One repository.** The simulation and the Godot layer over it change together. CMake builds the SDL3 game (the
  reference) and the GDExtension from the same sources.

## How it fits together

- The C engine builds as a static library. The SDL3 game is `i_main.c` plus that library, as before.
- `D_DoomMain` splits into setup and a loop, so that a host can run setup and then call one tic at a time.
- `I_Quit` and `I_Error` return to a host that asked for it instead of ending the process.
- The GDExtension (`godot/extension/`, C++ with godot-cpp) wraps the engine in one class, `DoomSim`: start it with
  command-line arguments, step it a tic, read the level and the things in it. It never writes game state.
- The Godot project (`godot/`) ticks `DoomSim` 35 times a second and draws what it reads. Doom's units become
  metres at 1/32: the player is 56 units, 1.75 m. Doom's x and y are Godot's x and -z; Doom's z is Godot's y.

## The first test

Two parts, to find out cheaply whether this works:

1. **The simulation runs inside Godot.** A v1.9 demo played in Godot headless writes the same `-hashlog` as the
   SDL3 build, tic for tic.
2. **A level shows in Godot.** E1M1's walls built as one mesh with the original textures, the things shown as
   boxes, and the camera following the demo's player.

**Result (October 2026): both parts work.** Played through `tools/run_demo.gd`, the three shareware demos log the
same `-hashlog` as the SDL3 build on every tic (5026, 3836 and 2134 tics, under a second each). `main.tscn` shows
the demos' levels with their walls and original textures, the things as boxes and the camera at the player's eye;
at tics 3 and 403 of demo1 it shows the same view as the SDL3 build's `-shotat`, wider for the wider window.

## After it

1. Floors and ceilings: polygons rebuilt from the BSP tree, since Doom never stored them. Sky.
2. Sector lighting, then Godot's: lights for lamps and fireballs, fog, glow.
3. Input, menus and the status bar, so the game can be played, not only watched.
4. Sound and music through Godot's audio.
5. Content: a definitions file mapping Doom's textures, flats and things to easel's PBR materials and GLB
   props, then models with animations for monsters once easel's character kind exists.
