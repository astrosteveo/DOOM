---
# solo-review settings. Every key is optional; these are the defaults.
depth: auto            # auto | quick | standard | deep
security: auto         # auto | always | never
check: "cmake -S . -B build && cmake --build build"
max_nits: 3
max_rounds: 3          # after this many rounds on one change, step back instead of re-reviewing
gate: push             # push | off: block Claude's `git push` while any finding is open
branch: require        # require | off: no file edits on the default branch
workflow: manual       # manual | auto: auto runs review and ship when a task is done; manual waits for you to ask
merge: rebase          # rebase | squash | merge: how /solo-review:ship lands a branch
high:                  # High risk: reviewed deep with a security pass, and you read them before a merge. Added to
                       # the plugin's always-High list (Claude config, CLAUDE.md, CI, git hooks, dependency
                       # manifests, secrets), which nothing here can remove. `ledger.py config` shows both.
  - "**/install.sh"
  - "**/release.sh"
  - "**/auth/**"
low:                   # Low risk: a change whose every file matches lands on a clean review and passing checks,
                       # without waiting for you. Empty until you list paths, such as "docs/**". It never lowers
                       # a High-risk file.
trusted:               # author emails besides your git user.email; a Low change with a commit by anyone else
                       # (a contributor's PR) waits for you
skip:                  # never reviewed, except files on the always-High list (such as lockfiles), which
                       # skip never hides
  - "**/*.min.js"
  - "**/*.snap"
  - "third_party/**"     # Nuked OPL3, vendored unmodified
---

# Review guide

## What Important means here
- A change touches game simulation (anything a demo replays), so a v1.9 demo stops matching the original code tic
  for tic under `-hashlog`. Only drawing, sound, input plumbing and the platform layer may differ. This holds for
  the Godot build as much as the SDL one: Godot reads the game's state and never writes it.
- Savegames stop reading or writing in the DOS v1.9 layout (`p_saveg.c`), or an old save loads wrong.
- A 64-bit bug: a pointer kept in an int, a size computed as count*4, or a fixed-point product that overflows.
- At a resolution other than 320x200 the renderer or a 2D drawer reads or writes past a buffer, or crashes.
- A command-line override (`-scale`, `-fullscreen`, `-window`, `-nograbmouse`) gets saved to the config.

## Always check

## Do not report
- Style, naming, formatting or refactoring ideas: linters and /simplify own those.
- Missing tests or docs, unless a line above asks for them.
- Anything a compiler, type checker or linter would catch.
- Generated and vendored files.

## Intentional
- `-fwrapv -fno-strict-aliasing` and the C17 pin (CMakeLists.txt): the 1997 code relies on wrapping and aliasing, and
  an optimizer that doesn't allow them changes demo playback.
- Original quirks in game logic (signed overflow, uninitialized-looking reads, odd math) stay as they are when demos
  depend on them.
- `ipx/`, `sersrc/` and `sndserv/` are the original DOS drivers and sound server, kept for history and not built.

## Evidence
- Cite file:line and the input or path that triggers each finding. Never guess from a name.
- Report a bug class once, listing every place it occurs.
