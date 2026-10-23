// DoomSim: the 1997 game, run a tic at a time inside Godot.
//
// It hands Godot the game's state, walls, things and textures as Doom
// has them, in map units; the scripts turn them into Godot's space.
// It reads the game and never writes it, so demos play as they do in
// the SDL3 build.

#ifndef DOOM_SIM_H
#define DOOM_SIM_H

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include <string>
#include <vector>

namespace godot {

class DoomSim : public Node {
	GDCLASS(DoomSim, Node)

public:
	// What start() and tic() return, as d_host.h's DH_RUNNING etc.
	enum Status {
		RUNNING = 0,
		QUIT = 1,
		ERROR = 2,
	};

	int start(const PackedStringArray &p_args);
	int tic();
	bool is_started() const { return started; }
	String get_error() const;

	Dictionary get_state() const;
	Dictionary get_walls() const;
	Dictionary get_things() const;
	String get_texture_name(int p_texture) const;
	Ref<Image> get_texture_image(int p_texture) const;

protected:
	static void _bind_methods();

private:
	bool started = false;
	// The game keeps pointers into argv, so the strings live here.
	std::vector<std::string> args;
	std::vector<char *> argv;
};

} // namespace godot

VARIANT_ENUM_CAST(DoomSim::Status);

#endif
