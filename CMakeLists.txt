cmake_minimum_required(VERSION 3.25)
project(linuxdoom LANGUAGES C)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# The engine predates C23 (unprototyped function pointers, 'boolean' enum),
# so pin the language level it was written against.
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 REQUIRED CONFIG)

set(SRC ${CMAKE_SOURCE_DIR}/linuxdoom-1.10)

add_executable(linuxdoom
  ${SRC}/i_main.c
  ${SRC}/i_system.c
  ${SRC}/i_video.c
  ${SRC}/i_sound.c
  ${SRC}/i_net.c
  ${SRC}/doomdef.c
  ${SRC}/doomstat.c
  ${SRC}/dstrings.c
  ${SRC}/tables.c
  ${SRC}/f_finale.c
  ${SRC}/f_wipe.c
  ${SRC}/d_main.c
  ${SRC}/d_iwad.c
  ${SRC}/d_net.c
  ${SRC}/d_items.c
  ${SRC}/g_game.c
  ${SRC}/m_menu.c
  ${SRC}/m_misc.c
  ${SRC}/m_argv.c
  ${SRC}/m_bbox.c
  ${SRC}/m_fixed.c
  ${SRC}/m_swap.c
  ${SRC}/m_cheat.c
  ${SRC}/m_random.c
  ${SRC}/am_map.c
  ${SRC}/p_ceilng.c
  ${SRC}/p_doors.c
  ${SRC}/p_enemy.c
  ${SRC}/p_floor.c
  ${SRC}/p_inter.c
  ${SRC}/p_lights.c
  ${SRC}/p_map.c
  ${SRC}/p_maputl.c
  ${SRC}/p_plats.c
  ${SRC}/p_pspr.c
  ${SRC}/p_setup.c
  ${SRC}/p_sight.c
  ${SRC}/p_spec.c
  ${SRC}/p_switch.c
  ${SRC}/p_mobj.c
  ${SRC}/p_telept.c
  ${SRC}/p_tick.c
  ${SRC}/p_saveg.c
  ${SRC}/p_user.c
  ${SRC}/r_bsp.c
  ${SRC}/r_data.c
  ${SRC}/r_draw.c
  ${SRC}/r_main.c
  ${SRC}/r_plane.c
  ${SRC}/r_segs.c
  ${SRC}/r_sky.c
  ${SRC}/r_things.c
  ${SRC}/w_wad.c
  ${SRC}/wi_stuff.c
  ${SRC}/v_video.c
  ${SRC}/st_lib.c
  ${SRC}/st_stuff.c
  ${SRC}/hu_stuff.c
  ${SRC}/hu_lib.c
  ${SRC}/s_sound.c
  ${SRC}/z_zone.c
  ${SRC}/info.c
  ${SRC}/sounds.c
)

target_compile_definitions(linuxdoom PRIVATE NORMALUNIX LINUX)
# The game logic was written for (and its demos recorded with) a compiler
# that let signed arithmetic wrap and didn't reason about aliasing; an
# optimizer that does can change how demos play back.
target_compile_options(linuxdoom PRIVATE -fwrapv -fno-strict-aliasing
  -Wall -Wno-unused-const-variable)
target_link_libraries(linuxdoom PRIVATE SDL3::SDL3 m)
