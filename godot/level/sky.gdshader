// The sky, on sky ceilings and the walls that show it, as R_DrawPlanes
// draws it: by the way each pixel looks, not mapped onto the surface,
// and at full brightness.
shader_type spatial;
render_mode unshaded;

uniform sampler2D sky : source_color, filter_nearest, repeat_enable;
// The top row's average, which looks above it fade to.
uniform vec3 above : source_color;

void fragment() {
	// Doom gives a turn 1024 columns, so a 256 wide sky goes round four
	// times, its columns counting up counterclockwise from east.
	vec3 look = (INV_VIEW_MATRIX * vec4(-VIEW, 0.0)).xyz;
	float turns = atan(-look.z, look.x) / TAU;
	vec2 size = vec2(textureSize(sky, 0));
	float u = turns * 1024.0 / size.x;
	// Row 100 at the horizon and 160 rows to a unit of height over depth
	// (skytexturemid, and pspriteiscale at any width). The game never
	// shows above the top row, so higher looks fade from it to the
	// colour above over 12 rows.
	float row = 100.0 - 160.0 * VERTEX.y / -VERTEX.z;
	float v = clamp(row, 0.5, size.y - 0.5) / size.y;
	ALBEDO = mix(texture(sky, vec2(u, v)).rgb, above, clamp(-row / 12.0, 0.0, 1.0));
}
