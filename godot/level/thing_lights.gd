extends Node3D
## Godot lights for the things that give light: lamps, torches, candles
## and burning barrels, and fireballs, rockets and plasma in flight and
## as they burst. Doom itself lights only by its sectors' light levels,
## which the level's surfaces glow with (surface.gdshader), so these add
## to that. A light per thing, from a pool that grows as it needs to,
## moved between tics with its thing. Fixtures cast shadows; projectiles,
## which come and go, don't.

const DoomSpace := preload("res://level/doom_space.gd")

const MF_MISSILE := 0x10000	# a projectile in flight, cleared as it bursts

const FIRE := Color(1.0, 0.55, 0.2)
const FLAME := Color(1.0, 0.75, 0.4)
const TECH := Color(0.85, 0.92, 1.0)
const GREEN := Color(0.4, 1.0, 0.35)
const BLUE := Color(0.4, 0.55, 1.0)
const RED := Color(1.0, 0.3, 0.2)

## What a light does besides shine.
const FLICKER := 1
const PROJECTILE := 2	# flies, bursts brighter, casts no shadow

## mobjtype_t: colour, energy, range in metres, how far up the thing
## (0 to 1) the light sits, and FLICKER and PROJECTILE.
const LIGHTS := {
	79: [TECH, 2.5, 9.0, 0.9, 0],	# MT_MISC29: tall tech lamp
	80: [TECH, 2.0, 7.0, 0.9, 0],	# MT_MISC30: short tech lamp
	81: [Color(1.0, 0.95, 0.8), 2.0, 7.0, 0.9, 0],	# MT_MISC31: floor lamp
	91: [BLUE, 2.5, 8.0, 0.85, FLICKER],	# MT_MISC41: tall blue torch
	92: [GREEN, 2.5, 8.0, 0.85, FLICKER],	# MT_MISC42: tall green torch
	93: [RED, 2.5, 8.0, 0.85, FLICKER],	# MT_MISC43: tall red torch
	94: [BLUE, 2.0, 6.0, 0.8, FLICKER],	# MT_MISC44: short blue torch
	95: [GREEN, 2.0, 6.0, 0.8, FLICKER],	# MT_MISC45: short green torch
	96: [RED, 2.0, 6.0, 0.8, FLICKER],	# MT_MISC46: short red torch
	99: [FLAME, 1.0, 3.0, 0.8, FLICKER],	# MT_MISC49: candle
	100: [FLAME, 2.0, 6.0, 0.95, FLICKER],	# MT_MISC50: candelabra
	127: [FIRE, 2.5, 8.0, 0.9, FLICKER],	# MT_MISC77: burning barrel
	88: [GREEN, 1.0, 4.0, 0.8, 0],	# MT_MISC38: evil eye
	89: [FIRE, 1.5, 5.0, 0.5, FLICKER],	# MT_MISC39: floating skull rock
	55: [BLUE, 1.0, 3.5, 0.5, 0],	# MT_MISC12: soulsphere
	56: [GREEN, 1.0, 3.5, 0.5, 0],	# MT_INV: invulnerability
	58: [RED, 0.8, 3.0, 0.5, 0],	# MT_INS: partial invisibility
	62: [TECH, 1.0, 3.5, 0.5, 0],	# MT_MEGA: megasphere
	18: [FIRE, 1.0, 4.0, 0.5, FLICKER],	# MT_SKULL: lost soul
	4: [FIRE, 2.5, 7.0, 0.5, FLICKER],	# MT_FIRE: arch-vile's fire
	29: [FIRE, 2.5, 7.0, 0.5, FLICKER],	# MT_SPAWNFIRE
	31: [FIRE, 2.0, 5.0, 0.5, PROJECTILE],	# MT_TROOPSHOT: imp fireball
	32: [Color(1.0, 0.35, 0.55), 2.0, 5.0, 0.5, PROJECTILE],	# MT_HEADSHOT: cacodemon ball
	16: [GREEN, 2.0, 5.0, 0.5, PROJECTILE],	# MT_BRUISERSHOT: baron ball
	33: [FIRE, 2.5, 6.0, 0.5, PROJECTILE],	# MT_ROCKET
	34: [BLUE, 2.0, 5.0, 0.5, PROJECTILE],	# MT_PLASMA
	35: [GREEN, 4.0, 10.0, 0.5, PROJECTILE],	# MT_BFG
	42: [GREEN, 2.0, 5.0, 0.5, PROJECTILE],	# MT_EXTRABFG
	36: [GREEN, 2.0, 5.0, 0.5, PROJECTILE],	# MT_ARACHPLAZ
	9: [FIRE, 2.0, 5.0, 0.5, PROJECTILE],	# MT_FATSHOT
	6: [FIRE, 2.0, 5.0, 0.5, PROJECTILE],	# MT_TRACER
	28: [FIRE, 2.0, 5.0, 0.5, PROJECTILE],	# MT_SPAWNSHOT
	39: [GREEN, 2.0, 5.0, 0.5, 0],	# MT_TFOG: teleport fog
	40: [GREEN, 1.0, 3.0, 0.5, 0],	# MT_IFOG: item respawn fog
}
## How much brighter and further a projectile lights as it bursts.
const BURST_ENERGY := 2.0
const BURST_RANGE := 1.5

var _lights: Array[OmniLight3D] = []
## Each light's flicker, or 0 for a steady one.
var _flicker := PackedFloat32Array()
var _energy := PackedFloat32Array()
## Each light's thing: its id, which keeps its flicker going the same
## way when the pool hands it another light, and where it is this tic
## and was the tic before, at the light's height.
var _ids := PackedInt32Array()
var _from := PackedVector3Array()
var _to := PackedVector3Array()
var _count := 0


## Whether things of this type give light, and in what colour.
static func gives_light(type: int) -> bool:
	return LIGHTS.has(type)


static func light_color(type: int) -> Color:
	return LIGHTS[type][0]


## Puts a light on each thing that gives one, from DoomSim.get_things(),
## for place() to move between tics.
func update(data: Dictionary) -> void:
	var coords: PackedFloat32Array = data["coords"]
	var old: PackedFloat32Array = data["old"]
	var types: PackedInt32Array = data["types"]
	var flags: PackedInt32Array = data["flags"]
	var ids: PackedInt32Array = data["ids"]

	var n := 0
	for i in types.size():
		if not LIGHTS.has(types[i]):
			continue
		var light: Array = LIGHTS[types[i]]
		var energy: float = light[1]
		var reach: float = light[2]
		var projectile: bool = light[4] & PROJECTILE
		if projectile and not (flags[i] & MF_MISSILE):
			energy *= BURST_ENERGY
			reach *= BURST_RANGE
		var c := i * 6
		var up := coords[c + 4] * float(light[3])
		var omni := _light(n)
		omni.light_color = light[0]
		omni.omni_range = reach
		omni.shadow_enabled = not projectile
		omni.visible = true
		_from[n] = DoomSpace.point(old[i * 4], old[i * 4 + 1], old[i * 4 + 2] + up)
		_to[n] = DoomSpace.point(coords[c], coords[c + 1], coords[c + 2] + up)
		_ids[n] = ids[i]
		_energy[n] = energy
		_flicker[n] = 1.0 if light[4] & FLICKER else 0.0
		omni.light_energy = energy
		n += 1
	for i in range(n, _lights.size()):
		_lights[i].visible = false
	_count = n


## Moves the lights fraction of the way from where their things were a
## tic before to where they are, as the things move.
func place(fraction: float) -> void:
	for i in _count:
		_lights[i].position = _from[i].lerp(_to[i], fraction)


func _process(_delta: float) -> void:
	# Flames flicker between tics. It's only light: the game never sees it.
	var t := Time.get_ticks_msec() / 1000.0
	for i in _count:
		if _flicker[i] > 0.0:
			var phase := float(_ids[i])
			var wave := 0.07 * sin(t * 13.0 + phase * 1.7) + 0.05 * sin(t * 31.0 + phase * 4.1)
			_lights[i].light_energy = _energy[i] * (1.0 + wave)


## The pool's nth light, made if there aren't that many yet.
func _light(n: int) -> OmniLight3D:
	while _lights.size() <= n:
		var omni := OmniLight3D.new()
		omni.name = "Light%d" % _lights.size()
		omni.distance_fade_enabled = true
		omni.distance_fade_begin = 40.0
		omni.distance_fade_length = 10.0
		# Placed every frame by place(); the pool's lights change things
		# from tic to tic, so Godot's interpolation would sweep them across.
		omni.physics_interpolation_mode = Node.PHYSICS_INTERPOLATION_MODE_OFF
		add_child(omni)
		_lights.append(omni)
		_flicker.append(0.0)
		_energy.append(0.0)
		_ids.append(0)
		_from.append(Vector3.ZERO)
		_to.append(Vector3.ZERO)
	return _lights[n]
