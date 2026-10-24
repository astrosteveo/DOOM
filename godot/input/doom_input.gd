extends Node
## The player's keys and mouse, posted to the game as Doom's own input
## events, as i_video.c posts SDL's, so the menus, the automap, cheats
## and the game's own key bindings all work as in the SDL3 build.
##
## Keys go by where they are on the keyboard, as Doom's do. The game's
## controls are also actions in Godot's Input Map (doom_forward and so
## on): a key bound to one there posts, besides itself, the key the
## game's config binds that control to, so Godot's bindings come on top
## of Doom's. In the menu, keys go as they are, to type savegame names.
##
## The mouse is the game's while a level is played, as in the SDL3 build:
## it turns and fires, and turns the view at once, between tics; menus,
## pauses and demos give the cursor back. -nograbmouse never takes it.
## The cursor works the menu, which reads where it is on Doom's screen:
## it picks what it is over, and clicks and the wheel go to that.

## Emitted when posting ends the game, as quitting from the menu does,
## with what DoomSim.post_event() returned.
signal ended(status: int)

## Input Map actions for the game's controls.
const CONTROLS := {
	&"doom_forward": DoomSim.CONTROL_FORWARD,
	&"doom_backward": DoomSim.CONTROL_BACKWARD,
	&"doom_turn_left": DoomSim.CONTROL_TURN_LEFT,
	&"doom_turn_right": DoomSim.CONTROL_TURN_RIGHT,
	&"doom_strafe_left": DoomSim.CONTROL_STRAFE_LEFT,
	&"doom_strafe_right": DoomSim.CONTROL_STRAFE_RIGHT,
	&"doom_fire": DoomSim.CONTROL_FIRE,
	&"doom_use": DoomSim.CONTROL_USE,
	&"doom_strafe": DoomSim.CONTROL_STRAFE,
	&"doom_run": DoomSim.CONTROL_RUN,
}

const DoomScreen := preload("res://screen/doom_screen.gd")

@export var sim: DoomSim
## For where the cursor is on Doom's screen.
var screen: DoomScreen

## The Doom keys each held key posted as it went down, by Godot keycode,
## so that letting go lets go of the same ones.
var _held: Dictionary[int, PackedInt32Array] = {}
var _buttons := 0
var _buttons_changed := false
## Mouse motion not yet posted, in pixels.
var _motion := Vector2.ZERO
var _grabbed := false
## The buttons held while the cursor works the menu, as a pointer event
## has them, and where it is on Doom's screen.
var _pointer_buttons := 0
var _pointer := Vector2i(-1, -1)
var _never_grab := OS.get_cmdline_user_args().has("-nograbmouse")


## Takes or gives back the mouse for the state the game is in, and posts
## the mouse's motion and buttons since the last frame as one event. Call
## it once a frame, before reading the view's turn from the game.
func update() -> void:
	var state := sim.get_state()
	var grab: bool = not _never_grab and get_window().has_focus() \
		and state["gamestate"] == 0 and not state["menu"] \
		and not state["paused"] and not state["demoplayback"]
	if grab != _grabbed:
		_grabbed = grab
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED if grab else Input.MOUSE_MODE_VISIBLE
		_motion = Vector2.ZERO
		if not grab and _buttons:
			_buttons = 0
			_buttons_changed = true
		# The game won't hear the button that started it come up, so
		# the menu must hear it now, to see the next click go down.
		if grab and _pointer_buttons:
			_pointer_buttons = 0
			_post_pointer()

	# Whole counts only, as i_video.c does; the rest waits for the next.
	var dx := int(_motion.x)
	var dy := int(_motion.y)
	_motion -= Vector2(dx, dy)
	if dx or dy or _buttons_changed:
		_buttons_changed = false
		_post(DoomSim.EVENT_MOUSE, _buttons, dx, -dy)


func _unhandled_input(event: InputEvent) -> void:
	var key := event as InputEventKey
	if key:
		_key(key)
		return
	if not _grabbed:
		_pointer_input(event)
		return
	var motion := event as InputEventMouseMotion
	if motion:
		_motion += motion.relative
		return
	var button := event as InputEventMouseButton
	if button:
		# Doom numbers buttons left, right, middle.
		var bit := 0
		match button.button_index:
			MOUSE_BUTTON_LEFT: bit = 1
			MOUSE_BUTTON_RIGHT: bit = 2
			MOUSE_BUTTON_MIDDLE: bit = 4
		if bit:
			_buttons = (_buttons | bit) if button.pressed else (_buttons & ~bit)
			_buttons_changed = true


func _notification(what: int) -> void:
	# Don't leave buttons held down while away.
	if what == NOTIFICATION_APPLICATION_FOCUS_OUT and _buttons:
		_buttons = 0
		_buttons_changed = true
	if what == NOTIFICATION_APPLICATION_FOCUS_OUT and _pointer_buttons:
		_pointer_buttons = 0
		_post_pointer()


## The cursor, for the menu, while the mouse isn't the game's.
func _pointer_input(event: InputEvent) -> void:
	var mouse := event as InputEventMouse
	if not mouse:
		return
	var at := screen.to_screen(mouse.position)
	var button := event as InputEventMouseButton
	if button:
		# Pointer events number the wheel's turns after the buttons.
		var bit := 0
		match button.button_index:
			MOUSE_BUTTON_LEFT: bit = 1
			MOUSE_BUTTON_RIGHT: bit = 2
			MOUSE_BUTTON_MIDDLE: bit = 4
			MOUSE_BUTTON_WHEEL_UP: bit = 8
			MOUSE_BUTTON_WHEEL_DOWN: bit = 16
		if not bit:
			return
		_pointer_buttons = (_pointer_buttons | bit) if button.pressed else (_pointer_buttons & ~bit)
	elif at == _pointer:
		return
	_pointer = at
	_post_pointer()


func _post_pointer() -> void:
	_post(DoomSim.EVENT_POINTER, _pointer_buttons, _pointer.x, _pointer.y)


func _key(event: InputEventKey) -> void:
	var code := event.physical_keycode
	if code == KEY_NONE:
		code = event.keycode
	var in_menu: bool = sim.get_state()["menu"]

	if not event.pressed:
		var keys: PackedInt32Array = _held.get(code, PackedInt32Array())
		_held.erase(code)
		for k in keys:
			_post(DoomSim.EVENT_KEY_UP, k)
		return

	# Held keys repeat only where the menus expect it, so that toggles
	# like the automap don't flicker.
	if event.echo:
		if in_menu:
			var raw := DoomSim.doom_key(code)
			if raw:
				_post(DoomSim.EVENT_KEY_DOWN, raw)
		return

	var keys := PackedInt32Array()
	var raw := DoomSim.doom_key(code)
	if raw:
		keys.append(raw)
	if not in_menu:
		for action: StringName in CONTROLS:
			if event.is_action(action):
				var bound := sim.get_control_key(CONTROLS[action])
				if bound and not keys.has(bound):
					keys.append(bound)
	if keys.is_empty():
		return
	_held[code] = keys
	for k in keys:
		_post(DoomSim.EVENT_KEY_DOWN, k)


func _post(type: DoomSim.EventType, data1: int, data2 := 0, data3 := 0) -> void:
	var status := sim.post_event(type, data1, data2, data3)
	if status != DoomSim.RUNNING:
		set_process_unhandled_input(false)
		ended.emit(status)
