#include "doom_sim.h"

#include <cstdlib>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>

#include "d_host.h"

using namespace godot;

static_assert(DoomSim::SKY == DH_SKY, "DoomSim::SKY is d_host.h's DH_SKY");

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
	d["sky"] = s.sky;
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
	PackedInt32Array sectors;
	coords.resize(count * 10);
	textures.resize(count);
	sectors.resize(count);
	for (int i = 0; i < count; i++) {
		const dh_wall_t &w = walls[i];
		const float values[10] = { w.x1, w.y1, w.x2, w.y2, w.bottom, w.top, w.u1, w.u2, w.v1, w.v2 };
		for (int j = 0; j < 10; j++) {
			coords[i * 10 + j] = values[j];
		}
		textures[i] = w.texture;
		sectors[i] = w.sector;
	}
	// coords: x1, y1, x2, y2, bottom, top, u1, u2, v1, v2 for each wall.
	d["coords"] = coords;
	d["textures"] = textures;
	d["sectors"] = sectors;
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

Dictionary DoomSim::get_sectors() const {
	Dictionary d;
	if (!started) {
		return d;
	}
	std::vector<dh_sector_t> sectors(DH_GetSectors(nullptr, 0));
	int count = DH_GetSectors(sectors.data(), (int)sectors.size());

	PackedFloat32Array heights;
	PackedInt32Array flats;
	PackedInt32Array lights;
	heights.resize(count * 2);
	flats.resize(count * 2);
	lights.resize(count);
	for (int i = 0; i < count; i++) {
		const dh_sector_t &s = sectors[i];
		heights[i * 2] = s.floorheight;
		heights[i * 2 + 1] = s.ceilingheight;
		flats[i * 2] = s.floorflat;
		flats[i * 2 + 1] = s.ceilingflat;
		lights[i] = s.light;
	}
	// heights and flats: the floor's, then the ceiling's, for each sector.
	d["heights"] = heights;
	d["flats"] = flats;
	d["lights"] = lights;
	return d;
}

Dictionary DoomSim::get_subsectors() const {
	Dictionary d;
	if (!started) {
		return d;
	}
	std::vector<dh_subsector_t> subsectors(DH_GetSubsectors(nullptr, 0));
	int count = DH_GetSubsectors(subsectors.data(), (int)subsectors.size());
	std::vector<dh_point_t> points(DH_GetSubsectorPoints(nullptr, 0));
	int numpoints = DH_GetSubsectorPoints(points.data(), (int)points.size());

	PackedInt32Array sectors;
	PackedInt32Array ranges;
	PackedVector2Array corners;
	sectors.resize(count);
	ranges.resize(count * 2);
	corners.resize(numpoints);
	for (int i = 0; i < count; i++) {
		sectors[i] = subsectors[i].sector;
		ranges[i * 2] = subsectors[i].first;
		ranges[i * 2 + 1] = subsectors[i].count;
	}
	for (int i = 0; i < numpoints; i++) {
		corners[i] = Vector2(points[i].x, points[i].y);
	}
	// ranges: the first of its corners and how many, for each subsector;
	// the corners go clockwise seen from above.
	d["sectors"] = sectors;
	d["ranges"] = ranges;
	d["corners"] = corners;
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

String DoomSim::get_flat_name(int p_flat) const {
	char name[9];
	if (!started || !DH_FlatInfo(p_flat, name)) {
		return String();
	}
	return String(name);
}

Ref<Image> DoomSim::get_flat_image(int p_flat) const {
	char name[9];
	if (!started || !DH_FlatInfo(p_flat, name)) {
		return Ref<Image>();
	}
	PackedByteArray rgba;
	rgba.resize(DH_FLATSIZE * DH_FLATSIZE * 4);
	DH_FlatRGBA(p_flat, rgba.ptrw());
	return Image::create_from_data(DH_FLATSIZE, DH_FLATSIZE, false, Image::FORMAT_RGBA8, rgba);
}

void DoomSim::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start", "args"), &DoomSim::start);
	ClassDB::bind_method(D_METHOD("tic"), &DoomSim::tic);
	ClassDB::bind_method(D_METHOD("is_started"), &DoomSim::is_started);
	ClassDB::bind_method(D_METHOD("get_error"), &DoomSim::get_error);
	ClassDB::bind_method(D_METHOD("get_state"), &DoomSim::get_state);
	ClassDB::bind_method(D_METHOD("get_walls"), &DoomSim::get_walls);
	ClassDB::bind_method(D_METHOD("get_things"), &DoomSim::get_things);
	ClassDB::bind_method(D_METHOD("get_sectors"), &DoomSim::get_sectors);
	ClassDB::bind_method(D_METHOD("get_subsectors"), &DoomSim::get_subsectors);
	ClassDB::bind_method(D_METHOD("get_texture_name", "texture"), &DoomSim::get_texture_name);
	ClassDB::bind_method(D_METHOD("get_texture_image", "texture"), &DoomSim::get_texture_image);
	ClassDB::bind_method(D_METHOD("get_flat_name", "flat"), &DoomSim::get_flat_name);
	ClassDB::bind_method(D_METHOD("get_flat_image", "flat"), &DoomSim::get_flat_image);

	BIND_ENUM_CONSTANT(RUNNING);
	BIND_ENUM_CONSTANT(QUIT);
	BIND_ENUM_CONSTANT(ERROR);
	BIND_CONSTANT(SKY);
}
