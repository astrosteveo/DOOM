extends Node3D
## Runs the 1997 game in DoomSim, a tic per physics tick (35 a second),
## and shows it: the level built whenever one loads, and every frame the
## things and the camera at the player's eye placed between the last two
## tics as Doom draws them, the view turned at once by the mouse. Floors,
## ceilings and walls move between tics in the level's shaders, which
## read how far between tics a frame is from tic_fraction.
##
## Doom's command line goes after "--": godot --path godot -- -warp 1 1
## With none, the game runs its title loop and plays its demos. -classic,
## which the game ignores, leaves out Godot's lights, fog and glow, to
## show the level lit only as Doom lights it, as the SDL3 build does.

@onready var sim: DoomSim = %Sim
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
	_check(sim.start(args))


func _process(_delta: float) -> void:
	var fraction := Engine.get_physics_interpolation_fraction()
	if fraction_override >= 0.0:
		fraction = fraction_override
	RenderingServer.global_shader_parameter_set("tic_fraction", fraction)
	if not sim.is_started():
		return
	var state := sim.get_state()
	if state["gamestate"] != 0:
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
