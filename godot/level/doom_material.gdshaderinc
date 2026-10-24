// A StandardMaterial3D's maps, as easel's textures and props come
// (DoomContent hands them over): base colour, a normal map, and
// occlusion, roughness and metallic each from a channel of a texture,
// usually one ORM map.
//
// Doom's light is ambient light, so it lights the base colour less the
// occlusion, as the surface's emission; Godot's lights then light the
// rest of the material.

uniform vec4 albedo_color : source_color = vec4(1.0);
uniform sampler2D albedo_texture : source_color, filter_linear_mipmap_anisotropic, repeat_enable, hint_default_white;
uniform bool normal_enabled = false;
uniform sampler2D normal_texture : hint_normal, filter_linear_mipmap_anisotropic, repeat_enable;
uniform float normal_scale = 1.0;
uniform float roughness = 1.0;
uniform sampler2D roughness_texture : filter_linear_mipmap_anisotropic, repeat_enable, hint_default_white;
uniform vec4 roughness_channel = vec4(1.0, 0.0, 0.0, 0.0);
uniform float metallic = 0.0;
uniform sampler2D metallic_texture : filter_linear_mipmap_anisotropic, repeat_enable, hint_default_white;
uniform vec4 metallic_channel = vec4(1.0, 0.0, 0.0, 0.0);
uniform bool ao_enabled = false;
uniform sampler2D ao_texture : filter_linear_mipmap_anisotropic, repeat_enable, hint_default_white;
uniform vec4 ao_channel = vec4(1.0, 0.0, 0.0, 0.0);

// The base colour, linear.
vec3 doom_material_albedo(vec2 uv) {
	return texture(albedo_texture, uv).rgb * albedo_color.rgb;
}

float doom_material_ao(vec2 uv) {
	return ao_enabled ? dot(texture(ao_texture, uv), ao_channel) : 1.0;
}

float doom_material_roughness(vec2 uv) {
	return roughness * dot(texture(roughness_texture, uv), roughness_channel);
}

float doom_material_metallic(vec2 uv) {
	return metallic * dot(texture(metallic_texture, uv), metallic_channel);
}
