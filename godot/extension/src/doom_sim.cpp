#include "doom_sim.h"

#include <cstdlib>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/rect2i.hpp>

#include "d_host.h"

using namespace godot;

static_assert(DoomSim::SKY == DH_SKY, "DoomSim::SKY is d_host.h's DH_SKY");

void DoomSim::set_mix_sound(bool p_mix) {
	ERR_FAIL_COND_MSG(started, "Choose how the sound plays before start().");
	DH_MixSound(p_mix);
}

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

static_assert(DoomSim::EVENT_KEY_DOWN == DH_KEYDOWN && DoomSim::EVENT_KEY_UP == DH_KEYUP && DoomSim::EVENT_MOUSE == DH_MOUSE,
		"DoomSim's event types are d_host.h's");
static_assert(DoomSim::CONTROL_FORWARD == DH_FORWARD && DoomSim::CONTROL_RUN == DH_RUN && DH_RUN + 1 == DH_NUMCONTROLS,
		"DoomSim's controls are d_host.h's");

int DoomSim::post_event(EventType p_type, int p_data1, int p_data2, int p_data3) {
	ERR_FAIL_COND_V_MSG(!started, ERROR, "start() the game first.");
	const dh_event_t event = { (int)p_type, p_data1, p_data2, p_data3 };
	return DH_PostEvent(&event);
}

int DoomSim::get_control_key(Control p_control) const {
	return started ? DH_ControlKey((int)p_control) : 0;
}

// Doom's key for a Godot key, by where it is on a US keyboard as
// i_video.c's TranslateScancode has it, so that bindings like WASD stay
// put whatever the layout: pass a physical keycode. 0 for keys Doom
// doesn't have.
int DoomSim::doom_key(Key p_key) {
	if (p_key >= KEY_A && p_key <= KEY_Z) {
		return 'a' + (p_key - KEY_A);
	}
	if (p_key >= KEY_0 && p_key <= KEY_9) {
		return '0' + (p_key - KEY_0);
	}
	if (p_key >= KEY_KP_0 && p_key <= KEY_KP_9) {
		return '0' + (p_key - KEY_KP_0);
	}
	if (p_key >= KEY_F1 && p_key <= KEY_F10) {
		return DH_KEY_F1 + (p_key - KEY_F1);
	}
	switch (p_key) {
		case KEY_LEFT: return DH_KEY_LEFTARROW;
		case KEY_RIGHT: return DH_KEY_RIGHTARROW;
		case KEY_UP: return DH_KEY_UPARROW;
		case KEY_DOWN: return DH_KEY_DOWNARROW;
		case KEY_ESCAPE: return DH_KEY_ESCAPE;
		case KEY_ENTER:
		case KEY_KP_ENTER: return DH_KEY_ENTER;
		case KEY_TAB: return DH_KEY_TAB;
		case KEY_F11: return DH_KEY_F11;
		case KEY_F12: return DH_KEY_F12;
		case KEY_BACKSPACE: return DH_KEY_BACKSPACE;
		case KEY_DELETE: return DH_KEY_DEL;
		case KEY_PAUSE: return DH_KEY_PAUSE;
		case KEY_EQUAL:
		case KEY_KP_ADD: return DH_KEY_EQUALS;
		case KEY_MINUS:
		case KEY_KP_SUBTRACT: return DH_KEY_MINUS;
		case KEY_SHIFT: return DH_KEY_RSHIFT;
		case KEY_CTRL: return DH_KEY_RCTRL;
		case KEY_ALT: return DH_KEY_RALT;
		case KEY_CAPSLOCK: return DH_KEY_CAPSLOCK;
		case KEY_HOME: return DH_KEY_HOME;
		case KEY_END: return DH_KEY_END;
		case KEY_PAGEUP: return DH_KEY_PGUP;
		case KEY_PAGEDOWN: return DH_KEY_PGDN;
		case KEY_INSERT: return DH_KEY_INS;
		case KEY_SPACE: return ' ';
		case KEY_APOSTROPHE: return '\'';
		case KEY_COMMA: return ',';
		case KEY_PERIOD:
		case KEY_KP_PERIOD: return '.';
		case KEY_SLASH:
		case KEY_KP_DIVIDE: return '/';
		case KEY_SEMICOLON: return ';';
		case KEY_BRACKETLEFT: return '[';
		case KEY_BRACKETRIGHT: return ']';
		case KEY_BACKSLASH: return '\\';
		case KEY_QUOTELEFT: return '`';
		case KEY_KP_MULTIPLY: return '*';
		default: return 0;
	}
}

String DoomSim::get_error() const {
	return String::utf8(DH_ErrorMessage());
}

void DoomSim::set_screen_size(const Vector2i &p_size) {
	DH_SetScreenSize(p_size.x, p_size.y);
}

int DoomSim::draw(float p_fraction) {
	ERR_FAIL_COND_V_MSG(!started, ERROR, "start() the game first.");
	return DH_Draw(p_fraction);
}

Dictionary DoomSim::get_screen() const {
	Dictionary d;
	if (!started) {
		return d;
	}
	dh_screen_t s;
	DH_GetScreen(&s);
	PackedByteArray rgba;
	rgba.resize((int64_t)s.width * s.height * 4);
	DH_ScreenRGBA(rgba.ptrw());
	// image: the screen, transparent where the 3D view shows; view: where
	// that is, in its pixels, empty when the screen covers it.
	d["image"] = Image::create_from_data(s.width, s.height, false, Image::FORMAT_RGBA8, rgba);
	d["view"] = Rect2i(s.viewx, s.viewy, s.viewwidth, s.viewheight);
	d["projection"] = s.projection;
	return d;
}

Color DoomSim::get_palette_tint(int p_palette) const {
	if (!started) {
		return Color(0, 0, 0, 0);
	}
	float rgb[3];
	float amount;
	DH_PaletteTint(p_palette, rgb, &amount);
	return Color(rgb[0], rgb[1], rgb[2], amount);
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
	d["extralight"] = s.extralight;
	d["fixedcolormap"] = s.fixedcolormap;
	d["oldx"] = s.oldx;
	d["oldy"] = s.oldy;
	d["oldz"] = s.oldz;
	d["oldangle"] = s.oldangle;
	d["turn"] = s.turn;
	d["menu"] = s.menu != 0;
	d["paused"] = s.paused != 0;
	d["demoplayback"] = s.demoplayback != 0;
	d["automap"] = s.automap != 0;
	d["palette"] = s.palette;
	d["sfxvolume"] = s.sfxvolume;
	d["musicvolume"] = s.musicvolume;
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
	PackedInt32Array planes;
	coords.resize(count * 10);
	textures.resize(count);
	sectors.resize(count);
	planes.resize(count * 3);
	for (int i = 0; i < count; i++) {
		const dh_wall_t &w = walls[i];
		const float values[10] = { w.x1, w.y1, w.x2, w.y2, w.bottom, w.top, w.u1, w.u2, w.v1, w.v2 };
		for (int j = 0; j < 10; j++) {
			coords[i * 10 + j] = values[j];
		}
		textures[i] = w.texture;
		sectors[i] = w.sector;
		planes[i * 3] = w.bottomplane;
		planes[i * 3 + 1] = w.topplane;
		planes[i * 3 + 2] = w.pegplane;
	}
	// coords: x1, y1, x2, y2, bottom, top, u1, u2, v1, v2 for each wall;
	// planes: bottom, top and the texture's, numbered sector*2 for a
	// floor and sector*2+1 for a ceiling.
	d["coords"] = coords;
	d["textures"] = textures;
	d["sectors"] = sectors;
	d["planes"] = planes;
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
	PackedFloat32Array old;
	PackedInt32Array types;
	PackedInt32Array flags;
	PackedInt32Array ids;
	coords.resize(count * 6);
	old.resize(count * 4);
	types.resize(count);
	flags.resize(count);
	ids.resize(count);
	for (int i = 0; i < count; i++) {
		const dh_thing_t &t = things[i];
		const float values[6] = { t.x, t.y, t.z, t.radius, t.height, t.angle };
		for (int j = 0; j < 6; j++) {
			coords[i * 6 + j] = values[j];
		}
		old[i * 4] = t.oldx;
		old[i * 4 + 1] = t.oldy;
		old[i * 4 + 2] = t.oldz;
		old[i * 4 + 3] = t.oldangle;
		types[i] = t.type;
		flags[i] = t.flags;
		ids[i] = t.id;
	}
	// coords: x, y, z, radius, height, angle for each thing; old: x, y,
	// z and angle a tic before.
	d["coords"] = coords;
	d["old"] = old;
	d["types"] = types;
	d["flags"] = flags;
	d["ids"] = ids;
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

static_assert(DoomSim::SOUND_LOCAL == DH_SOUND_LOCAL && DoomSim::SOUND_THING == DH_SOUND_THING
				&& DoomSim::SOUND_SECTOR == DH_SOUND_SECTOR && DoomSim::SOUND_POINT == DH_SOUND_POINT,
		"DoomSim's sound origins are d_host.h's");

Dictionary DoomSim::get_sounds() const {
	Dictionary d;
	if (!started) {
		return d;
	}
	std::vector<dh_sound_t> sounds(DH_GetSounds(nullptr, 0));
	int count = DH_GetSounds(sounds.data(), (int)sounds.size());

	PackedInt32Array handles;
	PackedInt32Array sfx;
	PackedInt32Array volumes;
	PackedInt32Array seps;
	PackedFloat32Array speeds;
	PackedInt32Array origins;
	PackedInt32Array ids;
	PackedFloat32Array coords;
	handles.resize(count);
	sfx.resize(count);
	volumes.resize(count);
	seps.resize(count);
	speeds.resize(count);
	origins.resize(count);
	ids.resize(count);
	coords.resize(count * 6);
	for (int i = 0; i < count; i++) {
		const dh_sound_t &s = sounds[i];
		handles[i] = s.handle;
		sfx[i] = s.sfx;
		volumes[i] = s.volume;
		seps[i] = s.sep;
		speeds[i] = s.speed;
		origins[i] = s.origin;
		ids[i] = s.id;
		const float values[6] = { s.x, s.y, s.z, s.oldx, s.oldy, s.oldz };
		for (int j = 0; j < 6; j++) {
			coords[i * 6 + j] = values[j];
		}
	}
	// coords: x, y, z, then x, y, z a tic before, for each sound;
	// origins: SOUND_LOCAL and so on, with ids the thing's or sector's.
	d["handles"] = handles;
	d["sfx"] = sfx;
	d["volumes"] = volumes;
	d["seps"] = seps;
	d["speeds"] = speeds;
	d["origins"] = origins;
	d["ids"] = ids;
	d["coords"] = coords;
	return d;
}

String DoomSim::get_sound_name(int p_sfx) const {
	char name[9];
	int rate, length;
	if (!started || !DH_SoundInfo(p_sfx, name, &rate, &length)) {
		return String();
	}
	return String(name);
}

Ref<AudioStreamWAV> DoomSim::get_sound(int p_sfx) const {
	char name[9];
	int rate, length;
	if (!started || !DH_SoundInfo(p_sfx, name, &rate, &length)) {
		return Ref<AudioStreamWAV>();
	}
	PackedByteArray samples;
	samples.resize(length);
	DH_SoundSamples(p_sfx, samples.ptrw());
	// Doom's samples are unsigned; Godot's 8-bit ones are signed.
	uint8_t *p = samples.ptrw();
	for (int i = 0; i < length; i++) {
		p[i] ^= 0x80;
	}
	Ref<AudioStreamWAV> stream;
	stream.instantiate();
	stream->set_format(AudioStreamWAV::FORMAT_8_BITS);
	stream->set_mix_rate(rate);
	stream->set_stereo(false);
	stream->set_data(samples);
	return stream;
}

Dictionary DoomSim::get_music() const {
	Dictionary d;
	dh_music_t m;
	if (!started || !DH_GetMusic(&m)) {
		return d;
	}
	d["name"] = String(m.name);
	d["looping"] = m.looping != 0;
	d["paused"] = m.paused != 0;
	return d;
}

int DoomSim::get_sample_rate() const {
	return started ? DH_SampleRate() : 0;
}

PackedVector2Array DoomSim::render_audio(int p_frames) {
	PackedVector2Array frames;
	ERR_FAIL_COND_V_MSG(!started, frames, "start() the game first.");
	if (p_frames <= 0) {
		return frames;
	}
	frames.resize(p_frames);
	static_assert(sizeof(Vector2) == 2 * sizeof(float), "Vector2 is two floats");
	DH_RenderAudio(reinterpret_cast<float *>(frames.ptrw()), p_frames);
	return frames;
}

void DoomSim::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mix_sound", "mix"), &DoomSim::set_mix_sound);
	ClassDB::bind_method(D_METHOD("start", "args"), &DoomSim::start);
	ClassDB::bind_method(D_METHOD("tic"), &DoomSim::tic);
	ClassDB::bind_method(D_METHOD("is_started"), &DoomSim::is_started);
	ClassDB::bind_method(D_METHOD("get_error"), &DoomSim::get_error);
	ClassDB::bind_method(D_METHOD("post_event", "type", "data1", "data2", "data3"), &DoomSim::post_event, DEFVAL(0), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_control_key", "control"), &DoomSim::get_control_key);
	ClassDB::bind_static_method("DoomSim", D_METHOD("doom_key", "key"), &DoomSim::doom_key);
	ClassDB::bind_method(D_METHOD("set_screen_size", "size"), &DoomSim::set_screen_size);
	ClassDB::bind_method(D_METHOD("draw", "fraction"), &DoomSim::draw);
	ClassDB::bind_method(D_METHOD("get_screen"), &DoomSim::get_screen);
	ClassDB::bind_method(D_METHOD("get_palette_tint", "palette"), &DoomSim::get_palette_tint);
	ClassDB::bind_method(D_METHOD("get_state"), &DoomSim::get_state);
	ClassDB::bind_method(D_METHOD("get_walls"), &DoomSim::get_walls);
	ClassDB::bind_method(D_METHOD("get_things"), &DoomSim::get_things);
	ClassDB::bind_method(D_METHOD("get_sectors"), &DoomSim::get_sectors);
	ClassDB::bind_method(D_METHOD("get_subsectors"), &DoomSim::get_subsectors);
	ClassDB::bind_method(D_METHOD("get_texture_name", "texture"), &DoomSim::get_texture_name);
	ClassDB::bind_method(D_METHOD("get_texture_image", "texture"), &DoomSim::get_texture_image);
	ClassDB::bind_method(D_METHOD("get_flat_name", "flat"), &DoomSim::get_flat_name);
	ClassDB::bind_method(D_METHOD("get_flat_image", "flat"), &DoomSim::get_flat_image);
	ClassDB::bind_method(D_METHOD("get_sounds"), &DoomSim::get_sounds);
	ClassDB::bind_method(D_METHOD("get_sound_name", "sfx"), &DoomSim::get_sound_name);
	ClassDB::bind_method(D_METHOD("get_sound", "sfx"), &DoomSim::get_sound);
	ClassDB::bind_method(D_METHOD("get_music"), &DoomSim::get_music);
	ClassDB::bind_method(D_METHOD("get_sample_rate"), &DoomSim::get_sample_rate);
	ClassDB::bind_method(D_METHOD("render_audio", "frames"), &DoomSim::render_audio);

	BIND_ENUM_CONSTANT(RUNNING);
	BIND_ENUM_CONSTANT(QUIT);
	BIND_ENUM_CONSTANT(ERROR);
	BIND_CONSTANT(SKY);
	BIND_ENUM_CONSTANT(EVENT_KEY_DOWN);
	BIND_ENUM_CONSTANT(EVENT_KEY_UP);
	BIND_ENUM_CONSTANT(EVENT_MOUSE);
	BIND_ENUM_CONSTANT(CONTROL_FORWARD);
	BIND_ENUM_CONSTANT(CONTROL_BACKWARD);
	BIND_ENUM_CONSTANT(CONTROL_TURN_LEFT);
	BIND_ENUM_CONSTANT(CONTROL_TURN_RIGHT);
	BIND_ENUM_CONSTANT(CONTROL_STRAFE_LEFT);
	BIND_ENUM_CONSTANT(CONTROL_STRAFE_RIGHT);
	BIND_ENUM_CONSTANT(CONTROL_FIRE);
	BIND_ENUM_CONSTANT(CONTROL_USE);
	BIND_ENUM_CONSTANT(CONTROL_STRAFE);
	BIND_ENUM_CONSTANT(CONTROL_RUN);
	BIND_ENUM_CONSTANT(SOUND_LOCAL);
	BIND_ENUM_CONSTANT(SOUND_THING);
	BIND_ENUM_CONSTANT(SOUND_SECTOR);
	BIND_ENUM_CONSTANT(SOUND_POINT);
}
