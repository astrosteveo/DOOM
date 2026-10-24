// A prop in place of a thing's sprite (content/definitions.cfg), lit
// as Doom lights the sprite: its sector's light, darker with depth by
// scalelight, or full bright for a full bright frame; Doom's light is
// the emission (doom_material.gdshaderinc) and Godot's lights add to
// it. Each instance's sector and frame come from DoomThingProps.
shader_type spatial;

#include "res://level/doom_light.gdshaderinc"
#include "res://level/doom_material.gdshaderinc"

global uniform sampler2D sector_lights;
instance uniform int sector = 0;
instance uniform bool fullbright = false;

void fragment() {
	int level = int(round(texelFetch(sector_lights, ivec2(sector % 256, sector / 256), 0).r * 255.0));
	int colormap = doom_colormap(level, -VERTEX.z * 32.0, 0, false);
	if (fullbright && fixedcolormap == 0) {
		colormap = 0;
	}
	vec3 color = doom_material_albedo(UV);
	float ao = doom_material_ao(UV);
	EMISSION = doom_shade(color * ao, colormap);
	ALBEDO = colormap == INVERSECOLORMAP ? EMISSION : color;
	if (normal_enabled) {
		NORMAL_MAP = texture(normal_texture, UV).rgb;
		NORMAL_MAP_DEPTH = normal_scale;
	}
	ROUGHNESS = doom_material_roughness(UV);
	METALLIC = doom_material_metallic(UV);
	AO = ao;
}
