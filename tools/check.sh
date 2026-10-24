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
log=$(godot --headless --path godot --quit-after 300 2>&1) || true
if printf '%s\n' "$log" | grep -E 'SCRIPT ERROR|Parse Error|Failed to load script'; then
	echo "check: the Godot project has script errors" >&2
	exit 1
fi
echo "check: builds and Godot scripts OK"
