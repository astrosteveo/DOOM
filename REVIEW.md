# Review instructions

## What Important means here

- A v1.9 demo logs a different `-hashlog` through Godot than through the SDL3 build, or a demo recorded in Godot with
  `-record` no longer plays back the same in the SDL3 build.
- The simulation plays differently from vanilla: a change outside the host layer alters game state, or the Godot
  layer or `DoomSim` writes game state it should only read.
- A savegame no longer loads in, or is no longer written as, the v1.9 format.
- `I_Error` or `I_Quit` ends the process under a host instead of returning to it, or Godot keeps running after the
  game ended.
- With `-classic`, the Godot build no longer draws what the SDL3 build's `-shotat` shows.
- A Godot script fails to load from a fresh checkout: `godot --path godot` prints script errors.

## Cap the nits

Report at most five Nits per review.

## Do not report

- Style, naming, formatting or refactoring suggestions.
- Anything CI already enforces: lint, formatting, type errors.
- Generated files and lockfiles: `build/`, `build-*/`, Godot's `.uid` files, and `godot/assets/`, which easel builds
  from `art/`.
- Vendored code in `third_party/`.

## Always check

- Godot scripts refer to each other with `preload` by path, not by `class_name`, which resolves through a gitignored
  cache.
- A change to the simulation, or to the events a host posts to it, keeps the three shareware demos' `-hashlog` the
  same through `tools/run_demo.gd` as through the SDL3 build.
