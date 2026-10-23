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

## Floors, ceilings and the sky

Doom never stored floor polygons: its renderer fills floors in between the walls on screen. `d_host.c` rebuilds
each subsector's convex shape from the BSP tree, a box around the level cut by the partition lines on the way down
to the leaf, then by the leaf's segs, which take off the void behind one-sided walls. Split segs end at vertices
rounded to whole units, so a seg cuts only what lies more than a unit behind it; that leaves no cracks, and at most
a unit of floor under a wall. Summed per sector with no slack, the shapes' areas match the areas the sectors' lines
enclose to 0.03% on E1M1 to E1M9.

- `level.gd` builds a floor and a ceiling node per sector, once a level, at height 0. A moving floor or ceiling
  moves its node to this tic's height, for culling, and the shader places the vertices between the old and new
  heights (see Light). An animated flat swaps its material, and nothing is rebuilt.
- Light stays out of the geometry. `surface.gdshader` reads each sector's light from a texture with a texel per
  sector, which walls, floors and ceilings share; a flickering light changes a texel. The wall mesh rebuilds only
  when a texture changes; doors and lifts move walls in the shaders, between tics (see Light). On demo1's E1M5,
  where lights change every tic, the median tic's script time went from 2.4 ms (rebuilding the walls every tic) to
  0.2 ms.
- `sky.gdshader` draws the sky as `R_DrawPlanes` does, by the way each pixel looks: four times round, row 100 at
  the horizon, 160 rows to a unit of slope. It covers sky ceilings and floors, and the upper walls between two sky
  sectors, which Doom leaves undrawn so the sky shows. Godot's taller view sees above the sky's top row, which
  Doom never shows, so that fades to the top row's average colour.

The three demos still log the same `-hashlog` through Godot as in the SDL3 build.

## Light

Two layers: Doom's own light, as the SDL3 build draws it, and Godot's lights over it.

- **Doom's light.** `doom_light.gdshaderinc` works out the COLORMAP row Doom would draw each pixel through, with the
  arithmetic of `R_InitLightTables` and `R_ExecuteSetViewSize`: the sector's light in 16 steps, the player's
  extralight, and the depth in front of the eye, walls by their scale (scalelight, `160/depth` in 16ths) and floors
  and ceilings by distance (zlight, in steps of 16 units). Walls along x are a step darker and along y a step
  brighter. The visor and invulnerability replace all of it with their fixed row (`fixedcolormap` in `dh_state_t`);
  the sky stays full bright.
- **A brightness, not a palette lookup.** Row c darkens the palette by (32 − c)/32, measured from the IWAD's
  COLORMAP, so the shader scales the texture by that rather than indexing COLORMAP, and it will apply to easel's
  textures as well. It scales sRGB values, as the rows were made: scaling linear colour took dark rooms nearly to
  black. Invulnerability's row is gray, 255 less the luma. At demo2's tic 1900, walls, floors and the dark room
  match the SDL3 build's `-shotat` within a few percent.
- **Godot's lights over it.** Surfaces are lit, with Doom's light as their emission and the Environment's ambient
  and reflected light off, so with no lamps about they look exactly as in Doom. `thing_lights.gd` puts an
  OmniLight on each thing that gives light, by type from `DH_GetThings`: lamps, torches, candles and burning
  barrels cast shadows, and flames flicker between tics; projectiles light their way without shadows and burst
  brighter once `MF_MISSILE` clears. A light on the camera flashes with extralight. Volumetric fog with no ambient
  term is lit only by those lights, which gives them halos, and glow takes what they light past white.
- **`-classic`** (after `--`; the game ignores it) leaves out the lights, fog and glow, to compare with the SDL3
  build. At demo2's tic 1900 it draws the same pixels as Doom's light alone.

Floors, ceilings and walls move between tics as the camera does. `DH_GetWalls` gives every wall section that can
show, empty or not, with the sector planes its bottom, top and texture follow, so the list changes only with
textures. The level keeps two textures of sector heights, this tic's and the tic before's; the shaders place each
wall and plane vertex between them by the physics interpolation fraction, so walls and planes stay joined, and a
door or lift rebuilds nothing. On demo1's E1M5 the median tic still takes 0.2 ms of script time.

Not yet: the things are still boxes that jump from tic to tic, and their lights with them; smoothing them needs a
way to tell a thing from one tic to the next, which can come with sprites or models. Shadows of walls a door or lift
moves keep the wall where it was until something else in the light's range moves, since the shaders move it.

## After it

1. ~~Floors and ceilings: polygons rebuilt from the BSP tree, since Doom never stored them. Sky.~~ Done, above.
2. ~~Sector lighting, then Godot's: lights for lamps and fireballs, fog, glow.~~ Done, above.
3. Input, menus and the status bar, so the game can be played, not only watched.
4. Sound and music through Godot's audio.
5. Content: a definitions file mapping Doom's textures, flats and things to easel's PBR materials and GLB
   props, then models with animations for monsters once easel's character kind exists.
