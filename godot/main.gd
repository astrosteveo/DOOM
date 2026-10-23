extends Node3D
## Runs the 1997 game in DoomSim, a tic per physics tick (35 a second),
## and shows it: the level built whenever one loads, and every frame the
## things and the camera at the player's eye placed between the last two
## tics as Doom draws them, the view turned at once by the mouse. Over it
## goes Doom's own 2D screen (DoomScreen): status bar, menus, automap,
## intermission and weapon, with the 3D view seen through where Doom
## draws it, and nothing else. The player's keys and mouse go to the game
## through Input (DoomInput). Floors, ceilings and walls move between
## tics in the level's shaders, which read how far between tics a frame
## is from tic_fraction.
##
## Doom's command line goes after "--": godot --path godot -- -warp 1 1
## With none, the game runs its title loop and plays its demos. -classic,
## which the game ignores, leaves out Godot's lights, fog and glow, to
## show the level lit only as Doom lights it, as the SDL3 build does.

@onready var sim: DoomSim = %Sim
@onready var input: DoomInput = %Input
@onready var screen: DoomScreen = %Screen
@onready var level: DoomLevel = %Level
@onready var camera: Camera3D = %Camera
@onready var muzzle_flash: OmniLight3D = %MuzzleFlash
@onready var environment: Environment = %Environment.environment

var _levelcount := 0
## How far between tics frames are drawn, when 0 to 1, for looking at
## them under gdh, whose fixed frame rate keeps it near 0; below 0,
## frames are drawn where they come.
var fraction_override := -1.0


func _ready() -> void:
	var args := PackedStringArray(["-nosound", "-nogui"])
	args.append_array(OS.get_cmdline_user_args())
	if args.has("-classic"):
		level.lights.visible = false
		environment.glow_enabled = false
		environment.volumetric_fog_enabled = false
	input.ended.connect(_check)
	if _check(sim.start(args)):
		screen.start()


func _process(_delta: float) -> void:
	var fraction := Engine.get_physics_interpolation_fraction()
	if fraction_override >= 0.0:
		fraction = fraction_override
	RenderingServer.global_shader_parameter_set("tic_fraction", fraction)
	if not sim.is_started() or not is_physics_processing():
		return
	input.update()
	var state := sim.get_state()
	if not _check(screen.draw(fraction, state["palette"])):
		return
	# The 3D view shows only where the screen leaves room for it.
	level.visible = screen.view.has_area()
	if state["gamestate"] != 0 or not level.visible:
		return
	level.place_things(fraction)
	_place_camera(state, fraction)


func _physics_process(_delta: float) -> void:
	if not _check(sim.tic()):
		return
	var state := sim.get_state()
	if state["gamestate"] != 0:
		return
	RenderingServer.global_shader_parameter_set("extralight", state["extralight"])
	RenderingServer.global_shader_parameter_set("fixedcolormap", state["fixedcolormap"])
	# A shot lights the room for as long as Doom's extralight lasts.
	muzzle_flash.visible = level.lights.visible and state["extralight"] > 0
	muzzle_flash.light_energy = 0.6 * state["extralight"]
	if state["levelcount"] != _levelcount:
		_levelcount = state["levelcount"]
		level.build(sim)
	else:
		level.update_walls(sim)
		level.update_planes(sim)
		level.update_things(sim)


## Puts the camera where R_SetupFrame puts Doom's view: fraction of the
## way from the eye a tic before to this tic's, and turned by the mouse
## turn the next tic will make, so that it turns at once.
func _place_camera(state: Dictionary, fraction: float) -> void:
	var angle := lerp_angle(state["oldangle"], state["angle"], fraction) + float(state["turn"])
	camera.global_transform = Transform3D(
		Basis(Vector3.UP, DoomSpace.yaw(angle)),
		DoomSpace.point(
			lerpf(state["oldx"], state["x"], fraction),
			lerpf(state["oldy"], state["y"], fraction),
			lerpf(state["oldz"], state["z"], fraction)))

	# Doom's view window, as a frustum through the whole window: 90
	# degrees across twice the projection, looking straight ahead at the
	# view window's middle. Pixels are square here, where Doom stretches
	# its rows 1.2 times, so the 3D view shows a little more above and
	# below than Doom's.
	var window := Vector2(get_window().size)
	var width := camera.near * screen.size.x / screen.projection
	var middle := Vector2(screen.view.get_center()) / Vector2(screen.size)
	camera.size = width
	camera.frustum_offset = Vector2(
		width * (0.5 - middle.x),
		width * window.y / window.x * (middle.y - 0.5))


## False once the game has ended, after quitting Godot the same way.
func _check(status: int) -> bool:
	if status == DoomSim.RUNNING:
		return true
	set_physics_process(false)
	if status == DoomSim.QUIT:
		get_tree().quit()
	else:
		push_error("Doom: %s" % sim.get_error())
		get_tree().quit(1)
	return false
