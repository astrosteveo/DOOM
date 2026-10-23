class_name DoomSound
extends Node3D
## Plays the game's sound effects as Godot's audio. The game decides
## what plays, as it does for the SDL3 build: a few channels, one sound
## to a thing, the more important cutting off the less, nothing beyond
## 1200 units. This plays what DoomSim.get_sounds() lists: a thing's
## sound from the thing, moving with it between tics, a door's or
## lift's from the middle of its sector, and the player's own and the
## menu's from nowhere.
##
## Godot's 3D audio says which way a sound comes from, in place of
## Doom's flat left and right; how loud it is with distance stays
## Doom's, which the game works out every tic: full within 160 units,
## fading in a straight line to nothing at 1200, where the game stops
## it. Godot's own falloff would make a monster across a room 6 to 12 dB
## quieter than in Doom, then cut it off at 1200 units while still
## clearly heard.

## Doom's volume in the middle: DMX gives each side half the sound, and
## the SDL3 build mixes sound effects at 0.9 of full scale (i_sound.c).
const LEVEL := 0.9 * 126.0 / 255.0
## Godot's 3D panning plays a sound straight ahead at 0.72 of a sound
## with no place (measured), so 3D sounds are turned up by as much: one
## in front of the player sounds as loud as Doom's.
const LEVEL_3D := LEVEL / 0.72

@export var sim: DoomSim

var _sfx_bus := AudioServer.get_bus_index(&"SFX")
## The streams by sound number, made as they are first wanted.
var _streams: Dictionary[int, AudioStreamWAV] = {}
## What plays, by the game's handle for it.
var _players: Dictionary[int, Node] = {}


## Starts the sounds the game started since the last frame, stops those
## it stopped, and puts each where it is fraction of the way from the
## last tic to the next. The menu's sound volume turns the SFX bus.
func update(fraction: float, state: Dictionary) -> void:
	AudioServer.set_bus_volume_linear(_sfx_bus, state["sfxvolume"] / 127.0)

	var sounds := sim.get_sounds()
	var handles: PackedInt32Array = sounds["handles"]
	var sfx: PackedInt32Array = sounds["sfx"]
	var volumes: PackedInt32Array = sounds["volumes"]
	var speeds: PackedFloat32Array = sounds["speeds"]
	var origins: PackedInt32Array = sounds["origins"]
	var coords: PackedFloat32Array = sounds["coords"]

	var listed: Dictionary[int, bool] = {}
	for i in handles.size():
		var handle := handles[i]
		listed[handle] = true
		var player: Node = _players.get(handle)
		var started := player == null
		if started:
			player = _make(sfx[i], speeds[i], origins[i] != DoomSim.SOUND_LOCAL)
			if player == null:
				continue
			_players[handle] = player
		if player is AudioStreamPlayer3D:
			# The game's volume less the menu's, which the bus has.
			player.volume_linear = LEVEL_3D * volumes[i] / maxf(state["sfxvolume"], 1.0)
			var c := i * 6
			player.position = DoomSpace.point(
				lerpf(coords[c + 3], coords[c], fraction),
				lerpf(coords[c + 4], coords[c + 1], fraction),
				lerpf(coords[c + 5], coords[c + 2], fraction))
		if started:
			player.play()

	for handle in _players.keys():
		if not listed.has(handle):
			_players[handle].queue_free()
			_players.erase(handle)


## A player for the sound, not started.
func _make(sfx: int, speed: float, positional: bool) -> Node:
	var stream: AudioStreamWAV = _streams.get(sfx)
	if stream == null:
		stream = sim.get_sound(sfx)
		if stream == null:
			return null
		_streams[sfx] = stream

	var player: Node
	if positional:
		var player3d := AudioStreamPlayer3D.new()
		player3d.attenuation_model = AudioStreamPlayer3D.ATTENUATION_DISABLED
		player = player3d
	else:
		player = AudioStreamPlayer.new()
		player.volume_linear = LEVEL
	player.stream = stream
	player.bus = &"SFX"
	player.pitch_scale = speed
	add_child(player)
	return player
