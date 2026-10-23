// A wall, floor or ceiling as Doom draws it: its texture at its
// sector's light, darker with distance (doom_light.gdshaderinc), at its
// sectors' heights between tics (doom_planes.gdshaderinc). The light
// comes from sector_lights, a texel per sector, so a flickering light
// changes a texture and not a mesh. UV2.x is the sector, rounded since
// interpolation can leave it a hair under; on a wall UV2.y is its fake
// contrast.
shader_type spatial;
render_mode unshaded;

#include "res://level/doom_light.gdshaderinc"
#include "res://level/doom_planes.gdshaderinc"

uniform sampler2D albedo : source_color, filter_nearest_mipmap, repeat_enable;
// 256 sectors a row, light levels 0 to 255 as 0 to 1.
global uniform sampler2D sector_lights;
// Floors and ceilings dim with distance by zlight, walls by scalelight.
uniform bool plane = false;

void vertex() {
	doom_place(VERTEX, UV, CUSTOM0, MODEL_MATRIX[3].xyz);
}

void fragment() {
	int sector = int(round(UV2.x));
	int level = int(round(texelFetch(sector_lights, ivec2(sector % 256, sector / 256), 0).r * 255.0));
	// View space is in metres, at 32 map units each.
	float depth = -VERTEX.z * 32.0;
	int colormap = doom_colormap(level, depth, int(round(UV2.y)), plane);
	ALBEDO = doom_shade(texture(albedo, UV).rgb, colormap);
}
