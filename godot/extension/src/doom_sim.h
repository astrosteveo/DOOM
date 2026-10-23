// DoomSim: the 1997 game, run a tic at a time inside Godot.
//
// It hands Godot the game's state, walls, sectors, things, textures and
// flats as Doom has them, in map units; the scripts turn them into
// Godot's space.
// It reads the game and never writes it; the player's keys and mouse
// go in as Doom's own input events, so demos play as they do in the
// SDL3 build.

#ifndef DOOM_SIM_H
#define DOOM_SIM_H

#include <godot_cpp/classes/global_constants.hpp>
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

	// A wall texture or flat that shows the sky, as d_host.h's DH_SKY.
	static const int SKY = -1;

	// What post_event() posts, as d_host.h's DH_KEYDOWN etc.
	enum EventType {
		EVENT_KEY_DOWN = 0,
		EVENT_KEY_UP = 1,
		EVENT_MOUSE = 2,
	};

	// The game's controls, as d_host.h's DH_FORWARD etc.
	enum Control {
		CONTROL_FORWARD,
		CONTROL_BACKWARD,
		CONTROL_TURN_LEFT,
		CONTROL_TURN_RIGHT,
		CONTROL_STRAFE_LEFT,
		CONTROL_STRAFE_RIGHT,
		CONTROL_FIRE,
		CONTROL_USE,
		CONTROL_STRAFE,
		CONTROL_RUN,
	};

	int start(const PackedStringArray &p_args);
	int tic();
	bool is_started() const { return started; }
	String get_error() const;

	int post_event(EventType p_type, int p_data1, int p_data2, int p_data3);
	int get_control_key(Control p_control) const;
	static int doom_key(Key p_key);

	Dictionary get_state() const;
	Dictionary get_walls() const;
	Dictionary get_things() const;
	Dictionary get_sectors() const;
	Dictionary get_subsectors() const;
	String get_texture_name(int p_texture) const;
	Ref<Image> get_texture_image(int p_texture) const;
	String get_flat_name(int p_flat) const;
	Ref<Image> get_flat_image(int p_flat) const;

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
VARIANT_ENUM_CAST(DoomSim::EventType);
VARIANT_ENUM_CAST(DoomSim::Control);

#endif
