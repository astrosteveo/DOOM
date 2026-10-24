// A thing's sprite as R_ProjectSprite places it: upright, along the
// view plane, a pixel to a map unit, with the thing's x, y and z at the
// lump's offsets. The mesh is a quad from (0, 0) to (1, 1); each
// instance stands at its thing's bottom middle, with INSTANCE_CUSTOM the
// lump, its flags (DOOM_SPRITE_FLIP, DOOM_SPRITE_FULLBRIGHT) and the
// sector it's in. Needs render_mode skip_vertex_transform.
//
// Doom draws sprites over floors and clips them only by the walls in
// front, so a sprite's pixels below the thing's feet still show over its
// floor. doom_sprite_depth puts those pixels at the depth of the floor
// they cover, a hair nearer, so the floor doesn't hide them and walls in
// front still do.
//
// Include doom_light.gdshaderinc first.

const int DOOM_SPRITE_FLIP = 1;
const int DOOM_SPRITE_FULLBRIGHT = 2;
// Nearer than this, in map units, Doom draws no sprite: the player's
// own is there.
const float DOOM_MINZ = 4.0;

// Palette indices, and alpha where the sprite has pixels.
uniform sampler2D atlas : filter_nearest;
// COLORMAP's rows as colours (DoomSim.get_colormap_image).
uniform sampler2D colormap : source_color, filter_nearest;
uniform vec2 atlas_size = vec2(1.0);
// Two texels a lump: where it is in the atlas, x, y, width and height
// in pixels, and below that its left and top offsets.
uniform sampler2D lumps : filter_nearest;

// Places the quad's corner, VERTEX on entry, in view space, and sets
// uv to where it is in the atlas, origin to the thing's bottom middle
// in view space, and its flags and sector. The shaders keep the last
// three in varyings for doom_sprite_depth and their light.
void doom_sprite_vertex(inout vec3 vertex, out vec2 uv, out vec3 origin, out int flags, out int sector,
		vec4 custom, mat4 modelview, mat4 view) {
	int lump = int(round(custom.x));
	flags = int(round(custom.y));
	sector = int(round(custom.z));
	vec4 rect = texelFetch(lumps, ivec2(lump, 0), 0);
	vec2 offset = texelFetch(lumps, ivec2(lump, 1), 0).xy;
	vec2 corner = vertex.xy;
	origin = (modelview * vec4(0.0, 0.0, 0.0, 1.0)).xyz;

	// Map units across the view plane from the thing, and up from its
	// feet; mirrored frames keep their offsets.
	vec2 at = vec2(corner.x * rect.z - offset.x, offset.y - (1.0 - corner.y) * rect.w);
	vec3 up = normalize(doom_up(view));
	vec3 across = normalize(cross(up, vec3(0.0, 0.0, 1.0)));
	vertex = origin + (across * at.x + up * at.y) / 32.0;
	if (doom_depth(origin, view) * 32.0 < DOOM_MINZ) {
		vertex = vec3(0.0, 0.0, 1.0);
	}

	float u = (flags & DOOM_SPRITE_FLIP) != 0 ? 1.0 - corner.x : corner.x;
	uv = (rect.xy + vec2(u, 1.0 - corner.y) * rect.zw) / atlas_size;
}

// The depth to draw a pixel of the sprite at, from its view space
// position: where it is, or the floor it covers below the thing's feet
// at origin.
float doom_sprite_depth(vec3 vertex, vec3 origin, float fragz, mat4 projection, mat4 view) {
	// Heights above the eye.
	vec3 up = doom_up(view);
	float feet = dot(origin, up);
	float height = dot(vertex, up);
	if (feet >= 0.0 || height >= feet) {
		return fragz;
	}
	// Along the same ray from the eye, at the height of the feet.
	vec3 floor_point = vertex * (feet / height) * 0.99;
	vec4 clip = projection * vec4(floor_point, 1.0);
	return clip.z / clip.w;
}
