extends Node3D
## Runs the 1997 game in DoomSim, a tic per physics tick (35 a second),
## and shows it: the level built whenever one loads, the things moved
## each tic, and the camera at the player's eye, smoothed between tics
## by physics interpolation. Floors, ceilings and walls are smoothed
## with it in the level's shaders, which read how far between tics a
## frame is from tic_fraction.
##
## Doom's command line goes after "--": godot --path godot -- -warp 1 1
## With none, the game runs its title loop and plays its demos.

@onready var sim: DoomSim = %Sim
@onready var level: DoomLevel = %Level
@onready var camera: Camera3D = %Camera

var _levelcount := 0


func _ready() -> void:
	var args := PackedStringArray(["-nosound", "-nogui"])
	args.append_array(OS.get_cmdline_user_args())
	_check(sim.start(args))


func _process(_delta: float) -> void:
	RenderingServer.global_shader_parameter_set("tic_fraction", Engine.get_physics_interpolation_fraction())


func _physics_process(_delta: float) -> void:
	if not _check(sim.tic()):
		return
	var state := sim.get_state()
	if state["gamestate"] != 0:
		return
	RenderingServer.global_shader_parameter_set("extralight", state["extralight"])
	RenderingServer.global_shader_parameter_set("fixedcolormap", state["fixedcolormap"])
	if state["levelcount"] != _levelcount:
		_levelcount = state["levelcount"]
		level.build(sim)
		_place_camera(state)
		camera.reset_physics_interpolation()
	else:
		level.update_walls(sim)
		level.update_planes(sim)
		level.update_things(sim)
		_place_camera(state)


func _place_camera(state: Dictionary) -> void:
	camera.global_transform = Transform3D(
		Basis(Vector3.UP, DoomSpace.yaw(state["angle"])),
		DoomSpace.point(state["x"], state["y"], state["z"]))


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
