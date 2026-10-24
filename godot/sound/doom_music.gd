extends AudioStreamPlayer
## Plays the game's music: Doom's MUS tracks on its emulated OPL3, which
## the game renders a frame's worth at a time into an
## AudioStreamGenerator. The score keeps time by the samples rendered,
## which is as fast as Godot's audio plays them, whatever the frame
## rate. The menu's music volume turns the Music bus down as DMX turns
## the music down, and the game's pause pauses the music, as in the SDL3
## build.
##
## An Ogg Vorbis file named after the track's lump, music/d_e1m1.ogg in
## the project or in user://, plays in its place.
##
## With mix, the game renders its sound effects into the same stream,
## at the menu's volumes, as the SDL3 build plays them; this plays it
## all on the Master bus, with no Ogg files.

## The music's level at each of the menu's 16 steps, in dB from full: DMX
## turns the music down by capping each MIDI channel's volume and taking
## that through its curve, measured on D_INTRO. Step 0 isn't silent.
const MENU_DB: Array[float] = [
	-38.2, -34.6, -30.9, -27.3, -22.7, -18.4, -15.1, -11.8,
	-9.4, -6.8, -5.2, -3.0, -1.1, -0.3, -0.2, 0.0]

## Rendered ahead of what plays: enough to cover a slow frame, and
## for the mix, little enough that a shot sounds with its flash.
const MUSIC_BUFFER := 0.2
const MIX_BUFFER := 0.1
const MUSIC_DIRS: Array[String] = ["user://music", "res://music"]

@export var sim: DoomSim

var _mix := false
var _music_bus := AudioServer.get_bus_index(&"Music")
## The track and the Ogg file playing in its place, if any.
var _track := ""
var _replacement: AudioStreamPlayer


## Nothing plays with -nosound, which leaves the game without a rate.
func start(mix: bool) -> void:
	_mix = mix
	if sim.get_sample_rate() <= 0:
		return
	var generator := AudioStreamGenerator.new()
	generator.mix_rate = sim.get_sample_rate()
	generator.buffer_length = MIX_BUFFER if mix else MUSIC_BUFFER
	stream = generator
	bus = &"Master" if mix else &"Music"
	play()
	_fill()


## Renders as much as has played since the last frame.
func update(state: Dictionary) -> void:
	if stream == null:
		return
	if not _mix:
		AudioServer.set_bus_volume_db(_music_bus, _menu_db(state["musicvolume"]))
		_follow_track()
		if _replacement != null:
			return
	_fill()


## Fills what has played. A generator that runs dry plays silence
## until it's filled again.
func _fill() -> void:
	var playback := get_stream_playback() as AudioStreamGeneratorPlayback
	var frames := playback.get_frames_available()
	if frames > 0:
		playback.push_buffer(sim.render_audio(frames))


## The menu sets the music volume in steps of 8 out of 127.
func _menu_db(volume: int) -> float:
	var step := volume / 8.0
	var i := clampi(floori(step), 0, MENU_DB.size() - 2)
	return lerpf(MENU_DB[i], MENU_DB[i + 1], clampf(step - i, 0.0, 1.0))


## Plays an Ogg file in place of the track, when there is one.
func _follow_track() -> void:
	var music := sim.get_music()
	var track: String = music.get("name", "")
	if track != _track:
		_track = track
		if _replacement != null:
			_replacement.queue_free()
			_replacement = null
		var ogg := _find_ogg(track)
		if ogg != null:
			ogg.loop = music["looping"]
			_replacement = AudioStreamPlayer.new()
			_replacement.stream = ogg
			_replacement.bus = &"Music"
			add_child(_replacement)
			_replacement.play()
	if _replacement != null:
		_replacement.stream_paused = music.get("paused", false)


func _find_ogg(track: String) -> AudioStreamOggVorbis:
	if track.is_empty():
		return null
	for dir in MUSIC_DIRS:
		var path := "%s/%s.ogg" % [dir, track.to_lower()]
		if dir.begins_with("res://") and ResourceLoader.exists(path):
			return load(path) as AudioStreamOggVorbis
		if FileAccess.file_exists(path):
			return AudioStreamOggVorbis.load_from_file(path)
	return null
