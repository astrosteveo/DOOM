#!/bin/sh
# Builds the SDL3 game and the GDExtension, then runs the Godot project's main
# scene headless for a few seconds and fails on any script error: GDScript
# compiles only when Godot loads it, so neither build sees a broken script.
set -e
cd "$(dirname "$0")/.."
cmake -S . -B build
cmake --build build
cmake -S . -B build-godot -DDOOM_GODOT=ON
cmake --build build-godot --target doomgodot
if ! command -v godot >/dev/null 2>&1; then
	echo "check: godot is not on PATH" >&2
	exit 1
fi
status=0
log=$(godot --headless --path godot --quit-after 300 2>&1) || status=$?
if [ "$status" -ne 0 ]; then
	printf '%s\n' "$log" | tail -n 20
	echo "check: godot exited with status $status" >&2
	exit 1
fi
# Godot can exit 0 with broken scripts or a missing extension, so read the log too.
if printf '%s\n' "$log" | grep -E 'SCRIPT ERROR|Parse Error|Failed to load script|Can.t open dynamic library|GDExtension'; then
	echo "check: the Godot project has script errors" >&2
	exit 1
fi
# The game's own start-up line proves the extension loaded and the scene ran.
if ! printf '%s\n' "$log" | grep -q 'ST_Init: Init status bar'; then
	printf '%s\n' "$log" | tail -n 20
	echo "check: the game did not start in Godot" >&2
	exit 1
fi
echo "check: builds and Godot scripts OK"
