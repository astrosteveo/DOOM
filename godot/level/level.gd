extends Node3D
## The level the game is on, built from what DoomSim reads out of it:
## its walls as one mesh with a surface per texture, its floors and
## ceilings as a node per sector for each, lit by their sectors' light
## levels as Doom lights them; the sky where Doom shows it; and
## its things with Doom's sprites (DoomThingSprites), moved smoothly
## between tics. Where content/definitions.cfg lists something else for
## a texture, flat or sprite (DoomContent), that is drawn instead, lit
## by Doom's light all the same: a material, or a prop (DoomThingProps).
## Light lives in a texture, so a change of light rebuilds nothing;
## surface.gdshader darkens it with distance as Doom does. So do sector
## heights, a tic's and the tic before's: the shaders move walls, floors
## and ceilings between them, so they move smoothly, and a door or lift
## rebuilds nothing either.

const DoomSpace := preload("res://level/doom_space.gd")
const DoomThingSprites := preload("res://level/thing_sprites.gd")
const DoomThingProps := preload("res://level/thing_props.gd")
const DoomThingLights := preload("res://level/thing_lights.gd")
const DoomContent := preload("res://content/doom_content.gd")

const SURFACE_SHADER := preload("res://level/surface.gdshader")
const SKY_SHADER := preload("res://level/sky.gdshader")
const FLAT_SIZE := 64.0
## Mesh flags for CUSTOM0 as four floats: the planes a vertex follows.
const CUSTOM0_FLOATS := Mesh.ARRAY_CUSTOM_RGBA_FLOAT << Mesh.ARRAY_FORMAT_CUSTOM0_SHIFT
## Sectors to a row in the sector textures the shaders read, light
## levels and heights, a texel per sector.
const SECTOR_ROW := 256

@onready var walls: MeshInstance3D = %Walls
@onready var planes: Node3D = %Planes
@onready var things: DoomThingSprites = %Things
@onready var props: DoomThingProps = %Props
@onready var lights: DoomThingLights = %Lights

var _content := DoomContent.new()
var _materials: Dictionary[int, ShaderMaterial] = {}
## Each wall texture's size in texels: walls' uvs run 0 to 1 across one
## repeat of it, whatever material is drawn in its place.
var _sizes: Dictionary[int, Vector2] = {}
var _textures := PackedInt32Array()

var _flat_materials: Dictionary[int, ShaderMaterial] = {}
var _sky_material := ShaderMaterial.new()
var _sky := -1
var _sector_rows := 1
var _sector_lights := PackedByteArray()
var _lights_texture: ImageTexture
## Floor and ceiling heights this tic and the tic before, and whether
## they differ.
var _tic_heights := PackedFloat32Array()
var _heights_texture: ImageTexture
var _old_heights_texture: ImageTexture
var _moving := false
## Each sector's floor and then its ceiling, null for a sector with no
## area, and what they were last placed at.
var _planes: Array[MeshInstance3D] = []
var _heights := PackedFloat32Array()
var _flats := PackedInt32Array()
var _levels := PackedInt32Array()


func _ready() -> void:
	_sky_material.shader = SKY_SHADER


## Builds the level DoomSim is on now.
func build(sim: DoomSim) -> void:
	var sky: int = sim.get_state()["sky"]
	if sky != _sky:
		_sky = sky
		var image := sim.get_texture_image(sky)
		_sky_material.set_shader_parameter("sky", ImageTexture.create_from_image(image))
		var above := Color(0, 0, 0)
		for x in image.get_width():
			above += image.get_pixel(x, 0) / image.get_width()
		_sky_material.set_shader_parameter("above", above)
	things.start(sim)
	_start_sectors(sim)
	_build_walls(sim, sim.get_walls())
	_build_planes(sim)
	update_things(sim)


## Rebuilds the walls if switches or animated textures have changed any
## since the last build. Doors and lifts move them in the shaders.
func update_walls(sim: DoomSim) -> void:
	var data := sim.get_walls()
	if data["textures"] != _textures:
		_build_walls(sim, data)


## Updates the sector textures, which walls and planes read, where
## heights and lights change, and changes flats where they animate.
## The plane nodes go to their heights this tic too, so that Godot
## culls them where they are. Nothing is rebuilt.
func update_planes(sim: DoomSim) -> void:
	var data := sim.get_sectors()
	var heights: PackedFloat32Array = data["heights"]
	var flats: PackedInt32Array = data["flats"]
	var levels: PackedInt32Array = data["lights"]

	# Once heights stop changing, the tic before's catch up with them.
	if heights != _tic_heights or _moving:
		_old_heights_texture.update(_heights_image(_tic_heights))
		_heights_texture.update(_heights_image(heights))
		_moving = heights != _tic_heights
		_tic_heights = heights

	if heights != _heights or flats != _flats:
		var all := heights.size() != _heights.size()
		for i in mini(_planes.size(), heights.size()):
			var plane := _planes[i]
			if plane == null:
				continue
			if all or heights[i] != _heights[i]:
				plane.position.y = heights[i] * DoomSpace.SCALE
			if all or flats[i] != _flats[i]:
				plane.material_override = _flat_material(sim, flats[i])
		_heights = heights
		_flats = flats

	if levels != _levels:
		for sector in mini(levels.size(), _sector_lights.size()):
			_sector_lights[sector] = levels[sector]
		_lights_texture.update(_lights_image())
		_levels = levels


## Reads where the things are this tic and were the tic before, and
## what they look like, for place_things to draw them in between, and
## the lights on those that give light, unless the lights are hidden.
func update_things(sim: DoomSim) -> void:
	var data := sim.get_things()
	things.update(sim, data, props.update(sim, data, _content))
	if lights.visible:
		lights.update(data)


## Puts the things and lights fraction of the way from where the things
## were a tic before to where they are this tic, as Doom draws them
## between tics, each thing seen from the eye at (view_x, view_y).
func place_things(fraction: float, view_x: float, view_y: float) -> void:
	things.place(fraction, view_x, view_y)
	props.place(fraction)
	if lights.visible:
		lights.place(fraction)


## Sets up the sector textures for the level DoomSim is on now, with
## heights that haven't moved yet.
func _start_sectors(sim: DoomSim) -> void:
	var data := sim.get_sectors()
	_sector_rows = maxi(1, ceili(data["lights"].size() / float(SECTOR_ROW)))
	_sector_lights = PackedByteArray()
	_sector_lights.resize(SECTOR_ROW * _sector_rows)
	_tic_heights = data["heights"]
	_moving = false
	_lights_texture = ImageTexture.create_from_image(_lights_image())
	_heights_texture = ImageTexture.create_from_image(_heights_image(_tic_heights))
	_old_heights_texture = ImageTexture.create_from_image(_heights_image(_tic_heights))
	RenderingServer.global_shader_parameter_set("sector_lights", _lights_texture)
	RenderingServer.global_shader_parameter_set("sector_heights", _heights_texture)
	RenderingServer.global_shader_parameter_set("sector_old_heights", _old_heights_texture)


func _build_walls(sim: DoomSim, data: Dictionary) -> void:
	var coords: PackedFloat32Array = data["coords"]
	var textures: PackedInt32Array = data["textures"]
	var sectors: PackedInt32Array = data["sectors"]
	var wall_planes: PackedInt32Array = data["planes"]
	var heights: PackedFloat32Array = sim.get_sectors()["heights"]
	_textures = textures

	# One surface per texture: positions, normals, uvs, sectors, planes.
	var surfaces: Dictionary[int, Array] = {}
	for i in textures.size():
		var texture := textures[i]
		if not surfaces.has(texture):
			surfaces[texture] = [PackedVector3Array(), PackedVector3Array(), PackedVector2Array(), PackedVector2Array(), PackedFloat32Array()]
		var size := Vector2.ONE
		if texture != DoomSim.SKY:
			_material(sim, texture)
			size = _sizes[texture]
		_add_wall(surfaces[texture], coords, i * 10, size, sectors[i], wall_planes, i * 3, heights)

	var mesh := ArrayMesh.new()
	for texture in surfaces:
		var arrays := []
		arrays.resize(Mesh.ARRAY_MAX)
		arrays[Mesh.ARRAY_VERTEX] = surfaces[texture][0]
		arrays[Mesh.ARRAY_NORMAL] = surfaces[texture][1]
		arrays[Mesh.ARRAY_TEX_UV] = surfaces[texture][2]
		arrays[Mesh.ARRAY_TEX_UV2] = surfaces[texture][3]
		arrays[Mesh.ARRAY_CUSTOM0] = surfaces[texture][4]
		mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays, [], {}, CUSTOM0_FLOATS)
		var surface := mesh.get_surface_count() - 1
		if texture == DoomSim.SKY:
			mesh.surface_set_material(surface, _sky_material)
			mesh.surface_set_name(surface, "sky")
		else:
			mesh.surface_set_material(surface, _material(sim, texture))
			mesh.surface_set_name(surface, sim.get_texture_name(texture))
	walls.mesh = mesh
	# The shaders move walls up and down from where they were built.
	walls.extra_cull_margin = 16.0


## One wall as two triangles, wound clockwise as seen from its front:
## the right of its direction, as in Doom. size is its texture's. Its
## vertices carry the planes they follow, for doom_planes.gdshaderinc,
## and v at no height above the plane its texture moves with, which is
## the same wherever the planes are.
func _add_wall(surface: Array, coords: PackedFloat32Array, c: int, size: Vector2, sector: int,
		wall_planes: PackedInt32Array, p: int, heights: PackedFloat32Array) -> void:
	var x1 := coords[c]
	var y1 := coords[c + 1]
	var x2 := coords[c + 2]
	var y2 := coords[c + 3]
	var bottom := coords[c + 4]
	var top := coords[c + 5]
	var u1 := coords[c + 6] / size.x
	var u2 := coords[c + 7] / size.x
	var bottom_plane := wall_planes[p]
	var top_plane := wall_planes[p + 1]
	var peg_plane := wall_planes[p + 2]
	var v := (coords[c + 8] + top - heights[peg_plane]) / size.y

	var a_top := DoomSpace.point(x1, y1, top)
	var b_top := DoomSpace.point(x2, y2, top)
	var b_bottom := DoomSpace.point(x2, y2, bottom)
	var a_bottom := DoomSpace.point(x1, y1, bottom)
	var normal := Vector3(y2 - y1, 0.0, x2 - x1).normalized()
	# Doom's fake contrast: walls along x a light step darker, along y
	# one brighter.
	var contrast := -1.0 if y1 == y2 else 1.0 if x1 == x2 else 0.0

	var positions: PackedVector3Array = surface[0]
	var normals: PackedVector3Array = surface[1]
	var uvs: PackedVector2Array = surface[2]
	var sectors: PackedVector2Array = surface[3]
	positions.append_array([a_top, b_top, b_bottom, a_top, b_bottom, a_bottom])
	uvs.append_array([Vector2(u1, v), Vector2(u2, v), Vector2(u2, v), Vector2(u1, v), Vector2(u2, v), Vector2(u1, v)])
	# The top stays above the bottom: Doom draws nothing where it isn't.
	var on_top := PackedFloat32Array([top_plane, bottom_plane, peg_plane, 1.0 / size.y])
	var on_bottom := PackedFloat32Array([bottom_plane, -1.0, peg_plane, 1.0 / size.y])
	var custom: PackedFloat32Array = surface[4]
	for corner in [on_top, on_top, on_bottom, on_top, on_bottom, on_bottom]:
		custom.append_array(corner)
	for i in 6:
		normals.append(normal)
		sectors.append(Vector2(sector, contrast))
	# Packed arrays come out of an Array as copies: put them back.
	surface[0] = positions
	surface[1] = normals
	surface[2] = uvs
	surface[3] = sectors
	surface[4] = custom


func _material(sim: DoomSim, texture: int) -> ShaderMaterial:
	if not _materials.has(texture):
		var name := sim.get_texture_name(texture)
		var image := sim.get_texture_image(texture)
		_sizes[texture] = Vector2(image.get_size())
		_materials[texture] = _content.surface(name, false)
		if _materials[texture] == null:
			_materials[texture] = _surface_material(name, image, false)
	return _materials[texture]


## A node for each sector's floor and ceiling, built at height 0 from
## its subsectors and moved to where the sector has them.
func _build_planes(sim: DoomSim) -> void:
	for plane in _planes:
		if plane != null:
			plane.queue_free()
	_planes.clear()

	# Each sector's subsectors as triangles, fanned from a corner.
	var data := sim.get_subsectors()
	var sectors: PackedInt32Array = data["sectors"]
	var ranges: PackedInt32Array = data["ranges"]
	var corners: PackedVector2Array = data["corners"]
	var shapes: Dictionary[int, PackedVector2Array] = {}
	for i in sectors.size():
		var first := ranges[i * 2]
		var count := ranges[i * 2 + 1]
		var triangles: PackedVector2Array = shapes.get(sectors[i], PackedVector2Array())
		for j in range(1, count - 1):
			triangles.append_array([corners[first], corners[first + j], corners[first + j + 1]])
		shapes[sectors[i]] = triangles

	_planes.resize(sim.get_sectors()["lights"].size() * 2)
	for sector in shapes:
		if not shapes[sector].is_empty():
			_planes[sector * 2] = _plane(shapes[sector], sector, true)
			_planes[sector * 2 + 1] = _plane(shapes[sector], sector, false)

	_heights = PackedFloat32Array()
	_flats = PackedInt32Array()
	_levels = PackedInt32Array()
	update_planes(sim)


## A floor or ceiling at height 0: the triangles, clockwise seen from
## above, face up for a floor and down for a ceiling. Flats lie on a
## grid fixed to the map, so a plane's uvs are where it is.
func _plane(triangles: PackedVector2Array, sector: int, is_floor: bool) -> MeshInstance3D:
	var count := triangles.size()
	var positions := PackedVector3Array()
	var normals := PackedVector3Array()
	var uvs := PackedVector2Array()
	var sectors := PackedVector2Array()
	var custom := PackedFloat32Array()
	positions.resize(count)
	normals.resize(count)
	uvs.resize(count)
	sectors.resize(count)
	custom.resize(count * 4)
	for i in count:
		# A ceiling turns each triangle the other way round.
		var p := triangles[i if is_floor else i - i % 3 + 2 - i % 3]
		positions[i] = DoomSpace.point(p.x, p.y, 0.0)
		normals[i] = Vector3.UP if is_floor else Vector3.DOWN
		uvs[i] = Vector2(p.x, -p.y) / FLAT_SIZE
		sectors[i] = Vector2(sector, 0.0)
		custom[i * 4] = sector * 2 + (0 if is_floor else 1)
		custom[i * 4 + 1] = -1.0
		custom[i * 4 + 2] = -1.0

	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = positions
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_TEX_UV] = uvs
	arrays[Mesh.ARRAY_TEX_UV2] = sectors
	arrays[Mesh.ARRAY_CUSTOM0] = custom
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays, [], {}, CUSTOM0_FLOATS)

	var plane := MeshInstance3D.new()
	plane.name = ("Floor%d" if is_floor else "Ceiling%d") % sector
	plane.mesh = mesh
	# The shader moves it between tics, from where it was a tic ago to
	# where the node is now, so the node itself mustn't interpolate.
	plane.physics_interpolation_mode = Node.PHYSICS_INTERPOLATION_MODE_OFF
	plane.extra_cull_margin = 1.0
	planes.add_child(plane)
	return plane


func _flat_material(sim: DoomSim, flat: int) -> ShaderMaterial:
	if flat == DoomSim.SKY:
		return _sky_material
	if not _flat_materials.has(flat):
		var name := sim.get_flat_name(flat)
		_flat_materials[flat] = _content.surface(name, true)
		if _flat_materials[flat] == null:
			_flat_materials[flat] = _surface_material(name, sim.get_flat_image(flat), true)
	return _flat_materials[flat]


## A wall texture, or a flat for a floor or ceiling, lit by its sector's
## light.
func _surface_material(label: String, image: Image, plane: bool) -> ShaderMaterial:
	image.generate_mipmaps()
	var material := ShaderMaterial.new()
	material.resource_name = label
	material.shader = SURFACE_SHADER
	material.set_shader_parameter("albedo", ImageTexture.create_from_image(image))
	material.set_shader_parameter("plane", plane)
	return material


func _lights_image() -> Image:
	return Image.create_from_data(SECTOR_ROW, _sector_rows, false, Image.FORMAT_R8, _sector_lights)


## Floor and ceiling heights as red and green.
func _heights_image(heights: PackedFloat32Array) -> Image:
	var texels := heights.duplicate()
	texels.resize(SECTOR_ROW * _sector_rows * 2)
	return Image.create_from_data(SECTOR_ROW, _sector_rows, false, Image.FORMAT_RGF, texels.to_byte_array())
