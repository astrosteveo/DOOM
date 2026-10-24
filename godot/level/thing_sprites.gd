extends Node3D
## The level's things, drawn with Doom's own sprites as R_ProjectSprite
## draws them: each thing's frame this tic, seen from the side the eye
## sees it from, mirrored where Doom mirrors it, standing where its
## offsets put it, lit by its sector, and moved between tics with the
## eye. Every sprite lump is packed into one atlas of palette indices
## when the game starts, which the shaders light through COLORMAP as
## Doom does, and the things are two MultiMeshes, one for those drawn lit and one
## for those drawn as fuzz (MF_SHADOW): doom_sprite.gdshaderinc places
## each from where its thing stands and which lump it shows.

const DoomSpace := preload("res://level/doom_space.gd")

const MF_NOSECTOR := 8	# not drawn: teleport exits and the like
const MF_SHADOW := 0x40000	# spectres and partial invisibility

const ATLAS_WIDTH := 2048
## Doom's angles for picking a view: R_ProjectSprite adds 22.5 degrees
## nine times, and each view is 45.
const VIEW_TURN := PI * 9.0 / 8.0
const VIEW_STEP := PI / 4.0
## Floats an instance takes in a MultiMesh's buffer: a transform's
## three rows, then custom data.
const STRIDE := 16
const FLIP := 1
const FULLBRIGHT := 2

@onready var sprites: MultiMeshInstance3D = %Sprites
@onready var shadows: MultiMeshInstance3D = %Shadows

## Each sprite frame's eight lumps and flips, by sprite * 256 + frame.
var _frames: Dictionary[int, Dictionary] = {}
var _packed := false
## DoomSim.get_things() this tic, and which things each MultiMesh draws.
var _things: Dictionary = {}
var _lit := PackedInt32Array()
var _fuzz := PackedInt32Array()


## Packs the sprites once: they are the WAD's, whatever the level.
func start(sim: DoomSim) -> void:
	if _packed:
		return
	_packed = true
	var count := sim.get_sprite_lump_count()
	var images: Array[Image] = []
	var offsets: Array[Vector2i] = []
	var order := range(count)
	for lump in count:
		images.append(sim.get_sprite_image(lump))
		offsets.append(sim.get_sprite_lump(lump)["offset"])
	# Shelves, tallest first, a pixel apart.
	order.sort_custom(func(a: int, b: int) -> bool: return images[a].get_height() > images[b].get_height())
	var places: Array[Vector2i] = []
	places.resize(count)
	var x := 0
	var y := 0
	var shelf := 0
	for lump: int in order:
		var size := images[lump].get_size()
		if x + size.x > ATLAS_WIDTH:
			x = 0
			y += shelf + 1
			shelf = 0
		places[lump] = Vector2i(x, y)
		x += size.x + 1
		shelf = maxi(shelf, size.y)

	var atlas := Image.create_empty(ATLAS_WIDTH, maxi(1, y + shelf), false, Image.FORMAT_LA8)
	var info := Image.create_empty(maxi(1, count), 2, false, Image.FORMAT_RGBAF)
	for lump in count:
		var size := images[lump].get_size()
		atlas.blit_rect(images[lump], Rect2i(Vector2i.ZERO, size), places[lump])
		info.set_pixel(lump, 0, Color(places[lump].x, places[lump].y, size.x, size.y))
		info.set_pixel(lump, 1, Color(offsets[lump].x, offsets[lump].y, 0, 0))

	var atlas_texture := ImageTexture.create_from_image(atlas)
	var info_texture := ImageTexture.create_from_image(info)
	var colormap := ImageTexture.create_from_image(sim.get_colormap_image())
	for drawn in [sprites, shadows]:
		var material: ShaderMaterial = drawn.material_override
		material.set_shader_parameter("atlas", atlas_texture)
		material.set_shader_parameter("atlas_size", Vector2(atlas.get_size()))
		material.set_shader_parameter("lumps", info_texture)
		material.set_shader_parameter("colormap", colormap)


## Reads the things this tic, for place() to draw between tics, but for
## those drawn as props, which taken marks.
func update(sim: DoomSim, things: Dictionary, taken: PackedByteArray) -> void:
	_things = things
	var flags: PackedInt32Array = things["flags"]
	var frames: PackedInt32Array = things["sprites"]
	_lit.clear()
	_fuzz.clear()
	for i in flags.size():
		if flags[i] & MF_NOSECTOR or taken[i]:
			continue
		var key := frames[i * 3] * 256 + frames[i * 3 + 1]
		if not _frames.has(key):
			_frames[key] = sim.get_sprite_frame(frames[i * 3], frames[i * 3 + 1])
		if _frames[key].is_empty():
			continue
		if flags[i] & MF_SHADOW:
			_fuzz.append(i)
		else:
			_lit.append(i)


## Puts the things fraction of the way from where they were a tic before
## to where they are, each showing the view of it seen from the eye at
## (view_x, view_y), in map units.
func place(fraction: float, view_x: float, view_y: float) -> void:
	if _things.is_empty():
		return
	_fill(sprites.multimesh, _lit, fraction, view_x, view_y)
	_fill(shadows.multimesh, _fuzz, fraction, view_x, view_y)


func _fill(multimesh: MultiMesh, shown: PackedInt32Array, fraction: float, view_x: float, view_y: float) -> void:
	var coords: PackedFloat32Array = _things["coords"]
	var old: PackedFloat32Array = _things["old"]
	var frames: PackedInt32Array = _things["sprites"]
	var sectors: PackedInt32Array = _things["sectors"]
	var buffer := PackedFloat32Array()
	buffer.resize(shown.size() * STRIDE)
	for n in shown.size():
		var i := shown[n]
		var c := i * 6
		var o := i * 4
		var x := lerpf(old[o], coords[c], fraction)
		var y := lerpf(old[o + 1], coords[c + 1], fraction)
		var z := lerpf(old[o + 2], coords[c + 2], fraction)
		var angle := lerp_angle(old[o + 3], coords[c + 5], fraction)

		var frame: Dictionary = _frames[frames[i * 3] * 256 + frames[i * 3 + 1]]
		var view := 0
		if frame["rotates"]:
			view = int(fposmod(atan2(y - view_y, x - view_x) - angle + VIEW_TURN, TAU) / VIEW_STEP) & 7
		var lump: int = frame["lumps"][view]
		var flags := FLIP if frame["flips"][view] else 0
		if frames[i * 3 + 2]:
			flags |= FULLBRIGHT

		var at := DoomSpace.point(x, y, z)
		var b := n * STRIDE
		buffer[b] = 1.0
		buffer[b + 3] = at.x
		buffer[b + 5] = 1.0
		buffer[b + 7] = at.y
		buffer[b + 10] = 1.0
		buffer[b + 11] = at.z
		buffer[b + 12] = lump
		buffer[b + 13] = flags
		buffer[b + 14] = sectors[i]
	if multimesh.instance_count != shown.size():
		multimesh.instance_count = shown.size()
	if shown.size() > 0:
		multimesh.buffer = buffer
