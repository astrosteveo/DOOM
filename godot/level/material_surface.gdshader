// A wall, floor or ceiling with a material in place of Doom's texture
// (content/definitions.cfg): placed and lit as surface.gdshader does
// Doom's own, with Doom's light on the material's base colour as its
// emission (doom_material.gdshaderinc). UV runs 0 to 1 across one
// repeat of the Doom texture or flat it replaces.
shader_type spatial;

#include "res://level/doom_light.gdshaderinc"
#include "res://level/doom_planes.gdshaderinc"
#include "res://level/doom_material.gdshaderinc"

global uniform sampler2D sector_lights;
uniform bool plane = false;

void vertex() {
	doom_place(VERTEX, UV, CUSTOM0, MODEL_MATRIX[3].xyz);
}

void fragment() {
	int sector = int(round(UV2.x));
	int level = int(round(texelFetch(sector_lights, ivec2(sector % 256, sector / 256), 0).r * 255.0));
	float depth = -VERTEX.z * 32.0;
	int colormap = doom_colormap(level, depth, int(round(UV2.y)), plane);
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
