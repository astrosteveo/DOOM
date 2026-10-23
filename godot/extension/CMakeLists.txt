# The Godot extension: the engine library behind a DoomSim node.
# Built with -DDOOM_GODOT=ON; the library lands in godot/bin, where
# godot/doom.gdextension looks for it.

include(FetchContent)

# GDExtensions load in later Godot versions than the one they were
# built for; 4.5 is godot-cpp's newest stable tag.
set(GODOTCPP_TARGET template_debug CACHE STRING "godot-cpp build target")
FetchContent_Declare(godot-cpp
  GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
  GIT_TAG godot-4.5-stable   # e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(godot-cpp)

add_library(doomgodot SHARED
  src/doom_sim.cpp
  src/register_types.cpp)

target_link_libraries(doomgodot PRIVATE godot-cpp doomengine)
# Keep the engine's C globals (lines, sides, players...) out of what
# the library exports.
target_link_options(doomgodot PRIVATE -Wl,--exclude-libs,ALL)

get_target_property(GODOTCPP_SUFFIX godot-cpp GODOTCPP_SUFFIX)
set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/godot/bin")
set_target_properties(doomgodot PROPERTIES
  CXX_STANDARD 17
  CXX_EXTENSIONS OFF
  CXX_VISIBILITY_PRESET hidden
  POSITION_INDEPENDENT_CODE ON
  LIBRARY_OUTPUT_DIRECTORY "$<1:${OUTPUT_DIR}>"
  PREFIX "lib"
  OUTPUT_NAME "doom${GODOTCPP_SUFFIX}")
