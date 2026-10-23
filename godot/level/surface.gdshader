// A wall, floor or ceiling as Doom draws it for now: its texture at its
// sector's light, unshaded. The light comes from sector_lights, a texel
// per sector, so a flickering light changes a texture and not a mesh.
// UV2.x is the sector, rounded since interpolation can leave it a hair
// under.
shader_type spatial;
render_mode unshaded;

uniform sampler2D albedo : source_color, filter_nearest_mipmap, repeat_enable;
// 256 sectors a row, light levels 0 to 255 as 0 to 1.
uniform sampler2D sector_lights : filter_nearest;

vec3 srgb_to_linear(vec3 c) {
	return mix(pow((c + 0.055) / 1.055, vec3(2.4)), c / 12.92, lessThan(c, vec3(0.04045)));
}

void fragment() {
	int sector = int(round(UV2.x));
	float light = texelFetch(sector_lights, ivec2(sector % 256, sector / 256), 0).r;
	ALBEDO = texture(albedo, UV).rgb * srgb_to_linear(vec3(light));
}
