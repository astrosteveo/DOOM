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

Things and their lights move between tics too, and so does the camera. The engine already keeps where each thing and
the eye were a tic before, for the SDL3 build's interpolation, so `DH_GetThings` and `DH_GetState` hand that over, the
same as now when nothing moved or the game stood still. Godot places the boxes, the lights and the camera in between
every frame, as `R_SetupFrame` and `R_ProjectSprite` do; the camera turns at once with the mouse (below). Each thing
also has an id (`mobj_t`'s `id`, not game state), which keeps a flame's flicker steady when the light pool hands it
another light, and will let sprites and models keep their animation.

Not yet: shadows of walls a door or lift moves keep the wall where it was until something else in the light's range
moves, since the shaders move it.

## Playing it

Godot plays the game as well as showing it: input goes in as Doom's own events, and Doom draws its own 2D screen.

- **Input.** `DH_PostEvent` hands the game an `event_t` and runs the responders at once, as `D_DoomLoop` does
  between tics in the SDL3 build: the menu answers, and `G_PendingTurn` turns the view with the mouse before the next
  tic (`dh_state_t`'s `turn`). A demo ignores it as it does there, since its tics come from the demo. `DoomInput`
  translates keys by where they are on the keyboard, as `i_video.c` does, so menus, cheats and the config's bindings
  work as in the SDL3 build. The controls are also Input Map actions (`doom_forward` and so on), which post the key
  the config binds each control to (`DH_ControlKey`): Godot's bindings come on top of Doom's. The mouse is captured
  while a level is played, and turns only, as with `mouse_novert`.
- **Doom's own 2D layer, not Godot UI.** The status bar, messages, menus, automap, intermission, finale and title
  pages all work at once and look exactly as in the SDL3 build, which a rebuild in Godot UI would have to earn screen
  by screen; a Godot HUD can still replace parts of it later. `DH_Draw` runs `D_Display` with `hostview` set: no
  window, no wipe, and in place of the 3D view, the view window filled with one colour under the player's weapon.
  To tell what else was drawn over the view (messages, the pause sign, the menu), it draws those again over a second
  colour; where the two differ, the view shows through. `DoomScreen` shows the result over the 3D view, sharp at any
  scale, and Godot's camera looks through Doom's view window: 90 degrees across, centred on the window, so a status
  bar or a smaller screen size leaves the view where Doom has it. The view still walks the BSP tree each frame, which
  marks the lines the player has seen for the automap.
- **The screen's size.** As wide as the window, like the SDL3 build's widescreen, and 400 rows (`-scale` changes it),
  copied to Godot every frame. Doom stretches its rows 1.2 times; Godot's 3D view has square pixels, so it shows a
  little more above and below than Doom's, while the 2D layer is stretched as Doom's.
- **Palette flashes.** Damage, pickups, berserk and the radiation suit tint everything, 3D and 2D, by the palette
  Doom would switch to (`ST_Palette`, in `dh_state_t`'s `palette`): each PLAYPAL palette is the first blended toward
  one colour, which `DH_PaletteTint` fits, and Godot blends a full-screen rectangle toward it in sRGB as the palette
  does.

A demo recorded in Godot with `-record`, walking, strafing, running, turning with the mouse, firing with the keyboard
and the mouse, using and pausing, plays back in the SDL3 build with the same `-hashlog` on every tic. As in vanilla
Doom, opening the menu while recording desyncs the demo: the menu stops the world in single player, and playback
doesn't. The demo's name, path included, must fit `demoname`'s 31 characters.

## Sound

The game still decides what plays; Godot plays it. No audio device of the game's opens inside Godot.

- **Which sounds, from the game.** s_sound keeps choosing channels as in 1997: a few channels (`snd_channels`), one
  sound to a thing, the more important cutting off the less, nothing beyond 1200 units. Those rules stay the code the
  SDL3 build runs, not a copy in the host that could drift from it. Under a host, i_sound mixes nothing: it keeps each
  sound's volume and pitch, a tic moves it on by a tic's samples, and a sound ends a tic after its last one, so the
  channel frees as the host finishes playing it. `DH_Tic` runs `S_UpdateSounds`, as `D_DoomLoop` does.
  `DH_GetSounds` lists what plays, each with a handle, its volume, and where it comes from: a thing (by id, moving
  between tics as the things do), a sector (doors and lifts, from the middle of the sector), a point (the switch return
  sound, from wherever `P_UpdateSpecials`'s stray pointer says, as in vanilla), or nowhere, for the player's own sounds
  and the menu's.
- **Where from, by Godot; how loud, by Doom.** `DoomSound` plays each sound from the WAD's samples, on an
  `AudioStreamPlayer3D` at its origin or an `AudioStreamPlayer` for local sounds. Godot gives the direction. Distance
  stays Doom's: full within 160 units, fading in a straight line to nothing at 1200. Godot's inverse distance made a
  monster across a room 6 to 12 dB quieter and then cut it off while still clearly heard. Levels were measured against
  the SDL3 build: over demo1's first fighting the two come out the same, within 0.01 dB.
- **Music.** The game renders its MUS music on the emulated OPL3 (`DH_RenderAudio`), and `DoomMusic` feeds it to an
  `AudioStreamGenerator` each frame, as much as has played. The score moves on by the samples rendered, so it keeps
  Godot's audio's time, whatever the frame rate. Rendered offline, it follows the SDL3 build's music within a few
  percent, note for note: not sample for sample, since the OPL3's resampler starts each song at another phase. Under
  gdh in real time, from 30 seconds to three minutes the generator didn't run dry once; it did in the first seconds,
  as gdh let the held game run, and a hitch there shows as a moment of silence. An Ogg file named after a track's lump,
  `music/d_e1m1.ogg` in the project or `user://`, plays in its place.
- **Volumes.** `default_bus_layout.tres` has Master (with a limiter), SFX and Music. The menu's sound volume turns the
  SFX bus, and the music volume the Music bus along DMX's own curve, measured: DMX caps each MIDI channel's volume, so
  step 4 of 15 is -22.7 dB, not the -12 dB of a straight line. Pausing pauses the music as DMX does, the instruments
  cut and the drums ringing out. `-nosound`, `-nosfx` and `-nomusic` work as in the SDL3 build.
- **`-classic`** has the game mix it all as the SDL3 build does, flat stereo at the menu's volumes, through the same
  mixing code, and plays that on Master.

The pitches of sounds come from `M_Random`, not `P_Random`, and nothing in the simulation reads what s_sound does, so
sound can't change how the game plays: `tools/run_demo.gd` now runs with sound on, and the three demos log the same
`-hashlog` as the SDL3 build.

## After it

1. ~~Floors and ceilings: polygons rebuilt from the BSP tree, since Doom never stored them. Sky.~~ Done, above.
2. ~~Sector lighting, then Godot's: lights for lamps and fireballs, fog, glow.~~ Done, above.
3. ~~Input, menus and the status bar, so the game can be played, not only watched.~~ Done, above.
4. ~~Sound and music through Godot's audio.~~ Done, above.
5. Content: a definitions file mapping Doom's textures, flats and things to easel's PBR materials and GLB
   props, then models with animations for monsters once easel's character kind exists.
