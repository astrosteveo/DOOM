shader_type canvas_item;
// Doom's screen scaled to the window with its pixels kept sharp: each
// pixel shows flat, blended with its neighbour only across the edge
// between them, so that rows and columns come out even at any scale,
// as the SDL3 build's smooth scaling does. The texture filters
// linearly.

void fragment() {
	vec2 size = vec2(textureSize(TEXTURE, 0));
	vec2 texel = UV * size;
	vec2 seam = floor(texel + 0.5);
	vec2 across = max(fwidth(texel), vec2(1e-5));
	texel = seam + clamp((texel - seam) / across, -0.5, 0.5);
	COLOR = texture(TEXTURE, texel / size);
}
