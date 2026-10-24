// Doom's light, as its renderer works it out (r_main.c, r_segs.c,
// r_plane.c), as a brightness on the texture rather than a COLORMAP
// lookup, so that it still works on other textures than the palette's.
//
// The renderer draws a wall or flat through one of COLORMAP's 32 rows,
// and row c darkens the palette by (32 - c) / 32. Which row depends on
// the sector's light in 16 steps, the player's extralight, and the
// distance: walls by their scale on screen (R_StoreWallRange), floors
// and ceilings by their distance (R_MapPlane). Both come down to the
// depth in front of the eye, level, since Doom's view never looks up or
// down: doom_depth, which free look's pitch leaves alone.

// The console player's, from DoomSim's state: steps of light added for
// a muzzle flash, and the COLORMAP row everything is drawn through
// instead, if not 0 (1 for the light amplification visor, 32 for
// invulnerability).
global uniform int extralight;
global uniform int fixedcolormap;

const int LIGHTLEVELS = 16;
const int LIGHTSEGSHIFT = 4;
const int NUMCOLORMAPS = 32;
const int MAXLIGHTSCALE = 48;
const int MAXLIGHTZ = 128;
const int DISTMAP = 2;
const int INVERSECOLORMAP = 32;

// Up, in view space.
vec3 doom_up(mat4 view) {
	return mat3(view) * vec3(0.0, 1.0, 0.0);
}

// How far a point in view space is in front of the eye, level, in
// metres: its view space depth when the view doesn't pitch.
float doom_depth(vec3 vertex, mat4 view) {
	vec3 up = doom_up(view);
	return dot(vertex, normalize(vec3(0.0, 0.0, -1.0) + up * up.z));
}

// The COLORMAP row for a sector light level (0 to 255) at depth map
// units in front of the eye. contrast is a wall's fake contrast: -1
// along x, 1 along y, else 0.
int doom_colormap(int level, float depth, int contrast, bool plane) {
	if (fixedcolormap != 0) {
		return fixedcolormap;
	}
	int lightnum = clamp((level >> LIGHTSEGSHIFT) + extralight + contrast, 0, LIGHTLEVELS - 1);
	int startmap = (LIGHTLEVELS - 1 - lightnum) * 2 * NUMCOLORMAPS / LIGHTLEVELS;
	int scale;
	if (plane) {
		// zlight: distance in steps of 16 units, each the scale of a
		// wall at the step's far end (R_InitLightTables).
		int index = int(min(depth / 16.0, float(MAXLIGHTZ - 1)));
		scale = 160 / (index + 1);
	} else {
		// scalelight: a wall's scale on a 320 wide screen, which is
		// 160 / depth, in 16ths (R_ExecuteSetViewSize, lightscalenorm).
		scale = int(min(2560.0 / max(depth, 1.0), float(MAXLIGHTSCALE - 1)));
	}
	return clamp(startmap - scale / DISTMAP, 0, NUMCOLORMAPS - 1);
}

vec3 doom_srgb_to_linear(vec3 c) {
	return mix(pow((c + 0.055) / 1.055, vec3(2.4)), c / 12.92, lessThan(c, vec3(0.04045)));
}

vec3 doom_linear_to_srgb(vec3 c) {
	return mix(1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, c * 12.92, lessThan(c, vec3(0.0031308)));
}

// albedo (linear) as drawn through a COLORMAP row. The rows darken the
// palette's sRGB values, so the brightness applies to those: scaling
// linear colour instead would take the darkest rows nearly to black.
// Invulnerability's row is gray, 255 less the colour's luma.
vec3 doom_shade(vec3 albedo, int colormap) {
	vec3 srgb = doom_linear_to_srgb(albedo);
	if (colormap == INVERSECOLORMAP) {
		return doom_srgb_to_linear(vec3(1.0 - dot(srgb, vec3(0.299, 0.587, 0.114))));
	}
	float bright = float(NUMCOLORMAPS - colormap) / float(NUMCOLORMAPS);
	return doom_srgb_to_linear(srgb * bright);
}
