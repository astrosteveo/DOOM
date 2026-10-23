#include "doom_sim.h"

#include <cstdlib>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>

#include "d_host.h"

using namespace godot;

int DoomSim::start(const PackedStringArray &p_args) {
	ERR_FAIL_COND_V_MSG(started, ERROR, "The game is already running; only one runs in a process.");
	started = true;

	// SDL is only used for timing and events inside the engine; it
	// mustn't take Godot's SIGINT and SIGTERM.
	setenv("SDL_NO_SIGNAL_HANDLERS", "1", 1);

	args.clear();
	args.push_back("linuxdoom");
	for (int64_t i = 0; i < p_args.size(); i++) {
		args.push_back(p_args[i].utf8().get_data());
	}
	argv.clear();
	for (std::string &arg : args) {
		argv.push_back(arg.data());
	}
	argv.push_back(nullptr);

	return DH_Start((int)args.size(), argv.data());
}

int DoomSim::tic() {
	ERR_FAIL_COND_V_MSG(!started, ERROR, "start() the game first.");
	return DH_Tic();
}

String DoomSim::get_error() const {
	return String::utf8(DH_ErrorMessage());
}

Dictionary DoomSim::get_state() const {
	dh_state_t s;
	DH_GetState(&s);
	Dictionary d;
	d["gametic"] = s.gametic;
	d["gamestate"] = s.gamestate;
	d["episode"] = s.episode;
	d["map"] = s.map;
	d["leveltime"] = s.leveltime;
	d["levelcount"] = s.levelcount;
	d["x"] = s.x;
	d["y"] = s.y;
	d["z"] = s.z;
	d["angle"] = s.angle;
	return d;
}

Dictionary DoomSim::get_walls() const {
	Dictionary d;
	if (!started) {
		return d;
	}
	std::vector<dh_wall_t> walls(DH_GetWalls(nullptr, 0));
	int count = DH_GetWalls(walls.data(), (int)walls.size());

	PackedFloat32Array coords;
	PackedInt32Array textures;
	PackedInt32Array lights;
	coords.resize(count * 10);
	textures.resize(count);
	lights.resize(count);
	for (int i = 0; i < count; i++) {
		const dh_wall_t &w = walls[i];
		const float values[10] = { w.x1, w.y1, w.x2, w.y2, w.bottom, w.top, w.u1, w.u2, w.v1, w.v2 };
		for (int j = 0; j < 10; j++) {
			coords[i * 10 + j] = values[j];
		}
		textures[i] = w.texture;
		lights[i] = w.light;
	}
	// coords: x1, y1, x2, y2, bottom, top, u1, u2, v1, v2 for each wall.
	d["coords"] = coords;
	d["textures"] = textures;
	d["lights"] = lights;
	return d;
}

Dictionary DoomSim::get_things() const {
	Dictionary d;
	if (!started) {
		return d;
	}
	std::vector<dh_thing_t> things(DH_GetThings(nullptr, 0));
	int count = DH_GetThings(things.data(), (int)things.size());

	PackedFloat32Array coords;
	PackedInt32Array types;
	PackedInt32Array flags;
	coords.resize(count * 6);
	types.resize(count);
	flags.resize(count);
	for (int i = 0; i < count; i++) {
		const dh_thing_t &t = things[i];
		const float values[6] = { t.x, t.y, t.z, t.radius, t.height, t.angle };
		for (int j = 0; j < 6; j++) {
			coords[i * 6 + j] = values[j];
		}
		types[i] = t.type;
		flags[i] = t.flags;
	}
	// coords: x, y, z, radius, height, angle for each thing.
	d["coords"] = coords;
	d["types"] = types;
	d["flags"] = flags;
	return d;
}

String DoomSim::get_texture_name(int p_texture) const {
	char name[9];
	int width, height;
	if (!started || !DH_TextureInfo(p_texture, name, &width, &height)) {
		return String();
	}
	return String(name);
}

Ref<Image> DoomSim::get_texture_image(int p_texture) const {
	char name[9];
	int width, height;
	if (!started || !DH_TextureInfo(p_texture, name, &width, &height)) {
		return Ref<Image>();
	}
	PackedByteArray rgba;
	rgba.resize((int64_t)width * height * 4);
	DH_TextureRGBA(p_texture, rgba.ptrw());
	return Image::create_from_data(width, height, false, Image::FORMAT_RGBA8, rgba);
}

void DoomSim::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start", "args"), &DoomSim::start);
	ClassDB::bind_method(D_METHOD("tic"), &DoomSim::tic);
	ClassDB::bind_method(D_METHOD("is_started"), &DoomSim::is_started);
	ClassDB::bind_method(D_METHOD("get_error"), &DoomSim::get_error);
	ClassDB::bind_method(D_METHOD("get_state"), &DoomSim::get_state);
	ClassDB::bind_method(D_METHOD("get_walls"), &DoomSim::get_walls);
	ClassDB::bind_method(D_METHOD("get_things"), &DoomSim::get_things);
	ClassDB::bind_method(D_METHOD("get_texture_name", "texture"), &DoomSim::get_texture_name);
	ClassDB::bind_method(D_METHOD("get_texture_image", "texture"), &DoomSim::get_texture_image);

	BIND_ENUM_CONSTANT(RUNNING);
	BIND_ENUM_CONSTANT(QUIT);
	BIND_ENUM_CONSTANT(ERROR);
}
