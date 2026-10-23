class_name DoomLevel
extends Node3D
## The level the game is on, built from what DoomSim reads out of it:
## its walls as one mesh with a surface per texture, its floors and
## ceilings as a node per sector for each, lit by their sectors' light
## levels as Doom draws them unshaded; the sky where Doom shows it; and
## its things as boxes their size for now.

const MT_PLAYER := 0
const MF_NOSECTOR := 8	# invisible things, such as teleport exits

const SURFACE_SHADER := preload("res://level/surface.gdshader")
const SKY_SHADER := preload("res://level/sky.gdshader")
const FLAT_SIZE := 64.0
## The light texture surface.gdshader reads: a texel per sector, as many
## sectors as a map can number.
const LIGHT_ROW := 256
const LIGHT_ROWS := 128

@onready var walls: MeshInstance3D = %Walls
@onready var planes: Node3D = %Planes
@onready var things: MultiMeshInstance3D = %Things

var _materials: Dictionary[int, StandardMaterial3D] = {}
var _coords := PackedFloat32Array()
var _textures := PackedInt32Array()
var _lights := PackedInt32Array()

var _flat_materials: Dictionary[int, ShaderMaterial] = {}
var _sky_material := ShaderMaterial.new()
var _sky := -1
var _sector_lights := PackedByteArray()
var _sector_lights_texture: ImageTexture
## Each sector's floor and then its ceiling, null for a sector with no
## area, and what they were last placed at.
var _planes: Array[MeshInstance3D] = []
var _heights := PackedFloat32Array()
var _flats := PackedInt32Array()
var _levels := PackedInt32Array()


func _ready() -> void:
	_sky_material.shader = SKY_SHADER
	_sector_lights.resize(LIGHT_ROW * LIGHT_ROWS)
	_sector_lights_texture = ImageTexture.create_from_image(_light_image())


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
	_build_walls(sim, sim.get_walls())
	_build_planes(sim)
	update_things(sim)


## Rebuilds the walls if doors, lifts, switches, lights or animated
## textures have changed any of them since the last build.
func update_walls(sim: DoomSim) -> void:
	var data := sim.get_walls()
	if data["coords"] != _coords or data["textures"] != _textures or data["lights"] != _lights:
		_build_walls(sim, data)


## Moves floors and ceilings to their sectors' heights, changes their
## flats where they animate, and their light where it changes. Nothing
## is rebuilt.
func update_planes(sim: DoomSim) -> void:
	var data := sim.get_sectors()
	var heights: PackedFloat32Array = data["heights"]
	var flats: PackedInt32Array = data["flats"]
	var levels: PackedInt32Array = data["lights"]

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
		_sector_lights_texture.update(_light_image())
		_levels = levels


## Moves the boxes to where the things are this tic.
func update_things(sim: DoomSim) -> void:
	var data := sim.get_things()
	var coords: PackedFloat32Array = data["coords"]
	var types: PackedInt32Array = data["types"]
	var flags: PackedInt32Array = data["flags"]

	var shown: Array[int] = []
	for i in types.size():
		if types[i] != MT_PLAYER and not (flags[i] & MF_NOSECTOR):
			shown.append(i)

	var multimesh := things.multimesh
	if multimesh.instance_count != shown.size():
		multimesh.instance_count = shown.size()
	for n in shown.size():
		var c := shown[n] * 6
		var radius := coords[c + 3]
		var height := coords[c + 4]
		var size := Vector3(radius * 2.0, height, radius * 2.0) * DoomSpace.SCALE
		var basis := Basis(Vector3.UP, DoomSpace.yaw(coords[c + 5])).scaled(size)
		var origin := DoomSpace.point(coords[c], coords[c + 1], coords[c + 2] + height / 2.0)
		multimesh.set_instance_transform(n, Transform3D(basis, origin))
		multimesh.set_instance_color(n, Color.from_hsv(fmod(types[shown[n]] * 0.618, 1.0), 0.6, 0.9))


func _build_walls(sim: DoomSim, data: Dictionary) -> void:
	var coords: PackedFloat32Array = data["coords"]
	var textures: PackedInt32Array = data["textures"]
	var lights: PackedInt32Array = data["lights"]
	_coords = coords
	_textures = textures
	_lights = lights

	# One surface per texture: positions, normals, uvs, colours.
	var surfaces: Dictionary[int, Array] = {}
	for i in textures.size():
		var texture := textures[i]
		if not surfaces.has(texture):
			surfaces[texture] = [PackedVector3Array(), PackedVector3Array(), PackedVector2Array(), PackedColorArray()]
		var size := Vector2.ONE
		if texture != DoomSim.SKY:
			size = Vector2(_material(sim, texture).albedo_texture.get_size())
		_add_wall(surfaces[texture], coords, i * 10, size, lights[i])

	var mesh := ArrayMesh.new()
	for texture in surfaces:
		var arrays := []
		arrays.resize(Mesh.ARRAY_MAX)
		arrays[Mesh.ARRAY_VERTEX] = surfaces[texture][0]
		arrays[Mesh.ARRAY_NORMAL] = surfaces[texture][1]
		arrays[Mesh.ARRAY_TEX_UV] = surfaces[texture][2]
		arrays[Mesh.ARRAY_COLOR] = surfaces[texture][3]
		mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
		var surface := mesh.get_surface_count() - 1
		if texture == DoomSim.SKY:
			mesh.surface_set_material(surface, _sky_material)
			mesh.surface_set_name(surface, "sky")
		else:
			mesh.surface_set_material(surface, _material(sim, texture))
			mesh.surface_set_name(surface, sim.get_texture_name(texture))
	walls.mesh = mesh


## One wall as two triangles, wound clockwise as seen from its front:
## the right of its direction, as in Doom. size is its texture's.
func _add_wall(surface: Array, coords: PackedFloat32Array, c: int, size: Vector2, light: int) -> void:
	var x1 := coords[c]
	var y1 := coords[c + 1]
	var x2 := coords[c + 2]
	var y2 := coords[c + 3]
	var bottom := coords[c + 4]
	var top := coords[c + 5]
	var u1 := coords[c + 6] / size.x
	var u2 := coords[c + 7] / size.x
	var v1 := coords[c + 8] / size.y
	var v2 := coords[c + 9] / size.y

	var a_top := DoomSpace.point(x1, y1, top)
	var b_top := DoomSpace.point(x2, y2, top)
	var b_bottom := DoomSpace.point(x2, y2, bottom)
	var a_bottom := DoomSpace.point(x1, y1, bottom)
	var normal := Vector3(y2 - y1, 0.0, x2 - x1).normalized()
	var shade := light / 255.0
	var color := Color(shade, shade, shade)

	var positions: PackedVector3Array = surface[0]
	var normals: PackedVector3Array = surface[1]
	var uvs: PackedVector2Array = surface[2]
	var colors: PackedColorArray = surface[3]
	positions.append_array([a_top, b_top, b_bottom, a_top, b_bottom, a_bottom])
	uvs.append_array([Vector2(u1, v1), Vector2(u2, v1), Vector2(u2, v2), Vector2(u1, v1), Vector2(u2, v2), Vector2(u1, v2)])
	for i in 6:
		normals.append(normal)
		colors.append(color)
	# Packed arrays come out of an Array as copies: put them back.
	surface[0] = positions
	surface[1] = normals
	surface[2] = uvs
	surface[3] = colors


func _material(sim: DoomSim, texture: int) -> StandardMaterial3D:
	if _materials.has(texture):
		return _materials[texture]
	var image := sim.get_texture_image(texture)
	image.generate_mipmaps()
	var material := StandardMaterial3D.new()
	material.resource_name = sim.get_texture_name(texture)
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_texture = ImageTexture.create_from_image(image)
	material.texture_filter = BaseMaterial3D.TEXTURE_FILTER_NEAREST_WITH_MIPMAPS
	material.vertex_color_use_as_albedo = true
	material.vertex_color_is_srgb = true
	_materials[texture] = material
	return material


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
	positions.resize(count)
	normals.resize(count)
	uvs.resize(count)
	sectors.resize(count)
	for i in count:
		# A ceiling turns each triangle the other way round.
		var p := triangles[i if is_floor else i - i % 3 + 2 - i % 3]
		positions[i] = DoomSpace.point(p.x, p.y, 0.0)
		normals[i] = Vector3.UP if is_floor else Vector3.DOWN
		uvs[i] = Vector2(p.x, -p.y) / FLAT_SIZE
		sectors[i] = Vector2(sector, 0.0)

	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = positions
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_TEX_UV] = uvs
	arrays[Mesh.ARRAY_TEX_UV2] = sectors
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)

	var plane := MeshInstance3D.new()
	plane.name = ("Floor%d" if is_floor else "Ceiling%d") % sector
	plane.mesh = mesh
	# The walls jump from tic to tic, so the planes they meet do too.
	plane.physics_interpolation_mode = Node.PHYSICS_INTERPOLATION_MODE_OFF
	planes.add_child(plane)
	return plane


func _flat_material(sim: DoomSim, flat: int) -> ShaderMaterial:
	if flat == DoomSim.SKY:
		return _sky_material
	if _flat_materials.has(flat):
		return _flat_materials[flat]
	var image := sim.get_flat_image(flat)
	image.generate_mipmaps()
	var material := ShaderMaterial.new()
	material.resource_name = sim.get_flat_name(flat)
	material.shader = SURFACE_SHADER
	material.set_shader_parameter("albedo", ImageTexture.create_from_image(image))
	material.set_shader_parameter("sector_lights", _sector_lights_texture)
	_flat_materials[flat] = material
	return material


func _light_image() -> Image:
	return Image.create_from_data(LIGHT_ROW, LIGHT_ROWS, false, Image.FORMAT_R8, _sector_lights)
