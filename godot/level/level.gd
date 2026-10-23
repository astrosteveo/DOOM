class_name DoomLevel
extends Node3D
## The level the game is on, built from what DoomSim reads out of it:
## its walls as one mesh with a surface per texture, lit by their
## sectors' light levels as Doom draws them unshaded, and its things as
## boxes their size for now.

const MT_PLAYER := 0
const MF_NOSECTOR := 8	# invisible things, such as teleport exits

@onready var walls: MeshInstance3D = %Walls
@onready var things: MultiMeshInstance3D = %Things

var _materials: Dictionary[int, StandardMaterial3D] = {}
var _coords := PackedFloat32Array()
var _textures := PackedInt32Array()
var _lights := PackedInt32Array()


## Builds the level DoomSim is on now.
func build(sim: DoomSim) -> void:
	_build_walls(sim, sim.get_walls())
	update_things(sim)


## Rebuilds the walls if doors, lifts, switches, lights or animated
## textures have changed any of them since the last build.
func update_walls(sim: DoomSim) -> void:
	var data := sim.get_walls()
	if data["coords"] != _coords or data["textures"] != _textures or data["lights"] != _lights:
		_build_walls(sim, data)


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
		_add_wall(surfaces[texture], coords, i * 10, _material(sim, texture), lights[i])

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
		mesh.surface_set_material(surface, _material(sim, texture))
		mesh.surface_set_name(surface, sim.get_texture_name(texture))
	walls.mesh = mesh


## One wall as two triangles, wound clockwise as seen from its front:
## the right of its direction, as in Doom.
func _add_wall(surface: Array, coords: PackedFloat32Array, c: int, material: StandardMaterial3D, light: int) -> void:
	var x1 := coords[c]
	var y1 := coords[c + 1]
	var x2 := coords[c + 2]
	var y2 := coords[c + 3]
	var bottom := coords[c + 4]
	var top := coords[c + 5]
	var size := Vector2(material.albedo_texture.get_size())
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
