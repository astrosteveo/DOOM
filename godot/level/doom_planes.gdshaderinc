// Floors and ceilings between tics: each wall and plane vertex goes to
// the height of the sector plane it follows, between where that was a
// tic ago and where it is now, as the camera goes between tics. The
// meshes stay as they are; a door or lift only changes two textures.

// Floor and ceiling heights in map units (red and green), a texel per
// sector, 256 to a row: this tic's and the tic before's.
global uniform sampler2D sector_heights;
global uniform sampler2D sector_old_heights;
// How far the frame is from the tic before to this one.
global uniform float tic_fraction;

// Plane sector*2 is that sector's floor, sector*2+1 its ceiling.
float doom_plane_height(int plane) {
	int sector = plane / 2;
	ivec2 at = ivec2(sector % 256, sector / 256);
	vec2 heights = mix(texelFetch(sector_old_heights, at, 0).rg, texelFetch(sector_heights, at, 0).rg, tic_fraction);
	return plane % 2 == 0 ? heights.r : heights.g;
}

// Moves a vertex (in the space of a node with no rotation or scale, at
// origin) up or down to its plane, from CUSTOM0: x is its plane; y, on
// the top of a wall, is the bottom's plane, which the top stays above,
// or else -1; z is the plane the wall's texture moves with, or -1; and
// w is 1 over the texture's height in texels.
void doom_place(inout vec3 vertex, inout vec2 uv, vec4 custom, vec3 origin) {
	float height = doom_plane_height(int(round(custom.x)));
	if (custom.y >= 0.0) {
		height = max(height, doom_plane_height(int(round(custom.y))));
	}
	// Map units are 1/32 m.
	vertex.y = height / 32.0 - origin.y;
	if (custom.z >= 0.0) {
		uv.y += (doom_plane_height(int(round(custom.z))) - height) * custom.w;
	}
}
