extends RefCounted
## What content/definitions.cfg puts in place of Doom's own textures,
## flats and sprites, made ready to draw: materials as ShaderMaterials
## that light them by Doom's light (material_surface.gdshader,
## prop.gdshader), props as scenes. Anything it doesn't list, or whose
## file is missing, it has nothing for, and Doom's own art is drawn.

const DEFINITIONS := "res://content/definitions.cfg"
const SURFACE_SHADER := preload("res://level/material_surface.gdshader")
const PROP_SHADER := preload("res://level/prop.gdshader")

var _paths: Dictionary[String, Dictionary] = {"textures": {}, "flats": {}, "sprites": {}}
var _surfaces: Dictionary[String, ShaderMaterial] = {}
var _props: Dictionary[String, PackedScene] = {}
var _prop_materials: Dictionary[Material, ShaderMaterial] = {}


func _init(path := DEFINITIONS) -> void:
	var config := ConfigFile.new()
	var error := config.load(path)
	if error != OK:
		push_warning("DoomContent: can't read %s (%s): drawing Doom's own art" % [path, error_string(error)])
		return
	for section in _paths:
		if config.has_section(section):
			for key in config.get_section_keys(section):
				_paths[section][key.to_upper()] = str(config.get_value(section, key))


## A material for a wall texture or a flat, by name, or null to draw
## Doom's own. plane: a floor or ceiling, which dims with distance as
## Doom's flats do.
func surface(name: String, plane: bool) -> ShaderMaterial:
	var section := "flats" if plane else "textures"
	var key := section + "/" + name
	if not _surfaces.has(key):
		_surfaces[key] = null
		var path: String = _paths[section].get(name, "")
		var standard := _load(path) as BaseMaterial3D
		if path != "" and standard == null:
			push_warning("DoomContent: %s for %s isn't a StandardMaterial3D" % [path, name])
		if standard != null:
			var material := _from_standard(standard, SURFACE_SHADER)
			material.resource_name = name
			material.set_shader_parameter("plane", plane)
			_surfaces[key] = material
	return _surfaces[key]


## Whether a sprite, by its four letter name, has a prop in its place.
func has_prop(sprite: String) -> bool:
	return _paths["sprites"].has(sprite)


## A prop for a sprite, its materials lit by Doom's light, or null.
## Each MeshInstance3D takes its thing's sector and whether its frame
## is full bright as instance uniforms (prop.gdshader).
func prop(sprite: String) -> Node3D:
	if not _props.has(sprite):
		var scene := _load(_paths["sprites"].get(sprite, "")) as PackedScene
		_props[sprite] = scene
	if _props[sprite] == null:
		return null
	var node := _props[sprite].instantiate() as Node3D
	for mesh: MeshInstance3D in node.find_children("*", "MeshInstance3D", true, false):
		for i in mesh.get_surface_override_material_count():
			var standard := mesh.get_active_material(i)
			if standard is BaseMaterial3D:
				if not _prop_materials.has(standard):
					_prop_materials[standard] = _from_standard(standard, PROP_SHADER)
				mesh.set_surface_override_material(i, _prop_materials[standard])
	return node


func _load(path: String) -> Resource:
	if path == "" or not ResourceLoader.exists(path):
		if path != "":
			push_warning("DoomContent: %s is missing: drawing Doom's own art" % path)
		return null
	return load(path)


## A StandardMaterial3D's maps on a shader that lights them by Doom's
## light (doom_material.gdshaderinc).
static func _from_standard(standard: BaseMaterial3D, shader: Shader) -> ShaderMaterial:
	var material := ShaderMaterial.new()
	material.shader = shader
	material.set_shader_parameter("albedo_color", standard.albedo_color)
	if standard.albedo_texture:
		material.set_shader_parameter("albedo_texture", standard.albedo_texture)
	material.set_shader_parameter("normal_enabled", standard.normal_enabled and standard.normal_texture != null)
	material.set_shader_parameter("normal_texture", standard.normal_texture)
	material.set_shader_parameter("normal_scale", standard.normal_scale)
	material.set_shader_parameter("roughness", standard.roughness)
	material.set_shader_parameter("metallic", standard.metallic)
	if standard.roughness_texture:
		material.set_shader_parameter("roughness_texture", standard.roughness_texture)
		material.set_shader_parameter("roughness_channel", _channel(standard.roughness_texture_channel))
	if standard.metallic_texture:
		material.set_shader_parameter("metallic_texture", standard.metallic_texture)
		material.set_shader_parameter("metallic_channel", _channel(standard.metallic_texture_channel))
	material.set_shader_parameter("ao_enabled", standard.ao_enabled and standard.ao_texture != null)
	if standard.ao_texture:
		material.set_shader_parameter("ao_texture", standard.ao_texture)
		material.set_shader_parameter("ao_channel", _channel(standard.ao_texture_channel))
	return material


## A BaseMaterial3D.TextureChannel as the vector that picks it.
static func _channel(channel: int) -> Vector4:
	if channel == BaseMaterial3D.TEXTURE_CHANNEL_GRAYSCALE:
		return Vector4(1.0, 1.0, 1.0, 0.0) / 3.0
	var picks := Vector4.ZERO
	picks[channel] = 1.0
	return picks
