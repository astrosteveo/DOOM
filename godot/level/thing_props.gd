class_name DoomThingProps
extends Node3D
## Props in place of the sprites content/definitions.cfg lists them for
## (DoomContent): a scene a thing, standing where the thing stands and
## turned the way it faces, moved between tics as the sprites are, and
## lit as Doom lights the sprite it replaces (prop.gdshader). A thing
## keeps its prop while its sprite is the listed one, so a state with
## another sprite, a barrel bursting, shows Doom's own.

const MF_NOSECTOR := 8	# not drawn: teleport exits and the like

## Each thing's prop and which sprite it stands for, by the thing's id.
var _props: Dictionary[int, Node3D] = {}
var _sprites: Dictionary[int, int] = {}
## Each sprite's four letter name, by number.
var _names: Dictionary[int, String] = {}
## Where each prop was a tic before and is this tic: x, y, z and angle.
var _from: Dictionary[int, Vector4] = {}
var _to: Dictionary[int, Vector4] = {}


## Puts a prop on each thing whose sprite has one, from
## DoomSim.get_things(), for place() to move between tics, and drops
## those whose things are gone or show another sprite. Returns which
## things have props, 1 a thing, for the sprites to leave out.
func update(sim: DoomSim, things: Dictionary, content: DoomContent) -> PackedByteArray:
	var coords: PackedFloat32Array = things["coords"]
	var old: PackedFloat32Array = things["old"]
	var types: PackedInt32Array = things["types"]
	var flags: PackedInt32Array = things["flags"]
	var ids: PackedInt32Array = things["ids"]
	var frames: PackedInt32Array = things["sprites"]
	var sectors: PackedInt32Array = things["sectors"]
	var taken := PackedByteArray()
	taken.resize(ids.size())

	var seen: Dictionary[int, bool] = {}
	for i in ids.size():
		if flags[i] & MF_NOSECTOR:
			continue
		var sprite := frames[i * 3]
		if not content.has_prop(_name(sim, sprite)):
			continue
		var id := ids[i]
		if _props.has(id) and _sprites[id] != sprite:
			_drop(id)
		if not _props.has(id):
			var node := content.prop(_name(sim, sprite))
			if node == null:
				continue
			# A thing that gives light lights from inside its prop.
			var shadows := not DoomThingLights.gives_light(types[i])
			for mesh: MeshInstance3D in node.find_children("*", "MeshInstance3D", true, false):
				mesh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON if shadows \
						else GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
			# Placed every frame by place().
			node.physics_interpolation_mode = Node.PHYSICS_INTERPOLATION_MODE_OFF
			add_child(node)
			_props[id] = node
			_sprites[id] = sprite
		for mesh: MeshInstance3D in _props[id].find_children("*", "MeshInstance3D", true, false):
			mesh.set_instance_shader_parameter("sector", sectors[i])
			mesh.set_instance_shader_parameter("fullbright", frames[i * 3 + 2] != 0)
		_from[id] = Vector4(old[i * 4], old[i * 4 + 1], old[i * 4 + 2], old[i * 4 + 3])
		_to[id] = Vector4(coords[i * 6], coords[i * 6 + 1], coords[i * 6 + 2], coords[i * 6 + 5])
		seen[id] = true
		taken[i] = 1

	for id in _props.keys():
		if not seen.has(id):
			_drop(id)
	return taken


## Puts the props fraction of the way from where their things were a
## tic before to where they are.
func place(fraction: float) -> void:
	for id in _props:
		var a := _from[id]
		var b := _to[id]
		_props[id].transform = Transform3D(
			Basis(Vector3.UP, DoomSpace.yaw(lerp_angle(a.w, b.w, fraction))),
			DoomSpace.point(lerpf(a.x, b.x, fraction), lerpf(a.y, b.y, fraction), lerpf(a.z, b.z, fraction)))


func _drop(id: int) -> void:
	_props[id].queue_free()
	_props.erase(id)
	_sprites.erase(id)
	_from.erase(id)
	_to.erase(id)


## A sprite's name: the first four letters of its lumps'.
func _name(sim: DoomSim, sprite: int) -> String:
	if not _names.has(sprite):
		var frame := sim.get_sprite_frame(sprite, 0)
		_names[sprite] = "" if frame.is_empty() else String(sim.get_sprite_lump(frame["lumps"][0])["name"]).left(4)
	return _names[sprite]
