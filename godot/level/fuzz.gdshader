// A spectre, or anything partly invisible (MF_SHADOW), as
// R_DrawFuzzColumn draws it: where its sprite has a pixel, what is
// behind it, through COLORMAP row 6, taken from an original sized pixel
// above or below by Doom's fuzz table. It has no light of its own.
shader_type spatial;
render_mode skip_vertex_transform, cull_disabled, unshaded, depth_draw_never;

#include "res://level/doom_light.gdshaderinc"
#include "res://level/doom_sprite.gdshaderinc"

varying flat vec3 origin;
varying flat int flags;
varying flat int sector;

uniform sampler2D screen : hint_screen_texture, filter_nearest;

// 1 a row below, -1 a row above (r_draw.c's fuzzoffset).
const int FUZZ[50] = {
	1, -1, 1, -1, 1, 1, -1, 1, 1, -1, 1, 1, 1, -1, 1, 1, 1, -1, -1, -1, -1,
	1, -1, -1, 1, 1, 1, 1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, -1, -1, -1, 1, 1,
	1, 1, -1, 1, 1, -1, 1
};

void vertex() {
	vec3 at;
	int drawn;
	int in_sector;
	doom_sprite_vertex(VERTEX, UV, at, drawn, in_sector, INSTANCE_CUSTOM, MODELVIEW_MATRIX, VIEW_MATRIX);
	origin = at;
	flags = drawn;
	sector = in_sector;
}

void fragment() {
	if (texture(atlas, UV).a < 0.5) {
		discard;
	}
	DEPTH = doom_sprite_depth(VERTEX, origin, FRAGCOORD.z, PROJECTION_MATRIX, VIEW_MATRIX);
	// Doom's 320x200, stretched over the view's height as its 2D layer
	// is. The table runs down each column and on into the next, and
	// goes on from where it left off each frame; this keeps its pattern
	// and lets it crawl.
	vec2 pixel = VIEWPORT_SIZE.y / vec2(240.0, 200.0);
	ivec2 original = ivec2(FRAGCOORD.xy / pixel);
	int start = original.x * 37 + int(TIME * 35.0) * 13;
	// Doom fuzzes a column from the top down, so a pixel that reads the
	// one above reads it already fuzzed, darkened again: follow the
	// table up the column while it says above and the sprite is there,
	// to the screen as it was behind it.
	vec2 row_up = dFdy(UV) * pixel.y;
	int rows = 0;
	int darker = 1;
	while (darker < 6 && FUZZ[(start + original.y - rows + 50) % 50] < 0
			&& texture(atlas, UV - row_up * float(rows + 1)).a >= 0.5) {
		rows++;
		darker++;
	}
	float shift = float(FUZZ[(start + original.y - rows + 50) % 50] - rows);
	vec3 behind = doom_linear_to_srgb(texture(screen, SCREEN_UV + vec2(0.0, shift / 200.0)).rgb);
	ALBEDO = doom_srgb_to_linear(behind * pow(float(NUMCOLORMAPS - 6) / float(NUMCOLORMAPS), float(darker)));
	ALPHA = 1.0;
}
