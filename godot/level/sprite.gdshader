// A thing as Doom draws it: its sprite (doom_sprite.gdshaderinc) at its
// sector's light, darker with the thing's depth as walls are, by
// scalelight with no fake contrast (R_AddSprites, R_ProjectSprite), or
// at full light for a full bright frame. As on walls, Doom's light is
// the emission and Godot's lights (thing_lights.gd) add to it.
//
// Unlike walls, which scale brightness so that other textures can take
// their place, sprites are always Doom's own palette art, so they go
// through COLORMAP itself: its rows shift hues as they darken, pinks
// toward tans and greens toward browns, which a brightness alone can't.
shader_type spatial;
render_mode skip_vertex_transform, cull_disabled;

#include "res://level/doom_light.gdshaderinc"
#include "res://level/doom_sprite.gdshaderinc"

varying flat vec3 origin;
varying flat int flags;
varying flat int sector;

global uniform sampler2D sector_lights;

void vertex() {
	vec3 at;
	int drawn;
	int in_sector;
	doom_sprite_vertex(VERTEX, UV, at, drawn, in_sector, INSTANCE_CUSTOM, MODELVIEW_MATRIX, VIEW_MATRIX);
	origin = at;
	flags = drawn;
	sector = in_sector;
	NORMAL = vec3(0.0, 0.0, 1.0);
}

void fragment() {
	vec4 texel = texture(atlas, UV);
	int index = int(round(texel.r * 255.0));
	ALPHA = texel.a;
	ALPHA_SCISSOR_THRESHOLD = 0.5;
	DEPTH = doom_sprite_depth(VERTEX, origin, FRAGCOORD.z, PROJECTION_MATRIX, VIEW_MATRIX);

	int level = int(round(texelFetch(sector_lights, ivec2(sector % 256, sector / 256), 0).r * 255.0));
	int row = doom_colormap(level, doom_depth(origin, VIEW_MATRIX) * 32.0, 0, false);
	if ((flags & DOOM_SPRITE_FULLBRIGHT) != 0 && fixedcolormap == 0) {
		row = 0;
	}
	EMISSION = texelFetch(colormap, ivec2(index, row), 0).rgb;
	ALBEDO = row == INVERSECOLORMAP ? EMISSION : texelFetch(colormap, ivec2(index, 0), 0).rgb;
	ROUGHNESS = 0.85;
	SPECULAR = 0.3;
}
