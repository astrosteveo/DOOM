extends SceneTree
## Runs the game through DoomSim as fast as it goes, with no window, for
## comparing -hashlog with the SDL3 build:
##
##   godot --headless --path godot -s res://tools/run_demo.gd -- -playdemo demo1 -hashlog demo1.txt
##
## Everything after "--" is Doom's command line. Exits 0 when the game
## quits (as -playdemo does when the demo ends), 1 on an error. The game's
## sound runs, which must not change how the demo plays, though nothing
## plays it; -nosound turns it off.


func _initialize() -> void:
	var sim := DoomSim.new()
	var args := PackedStringArray(["-nogui"])
	args.append_array(OS.get_cmdline_user_args())

	var status := sim.start(args)
	var tics := 0
	while status == DoomSim.RUNNING:
		status = sim.tic()
		tics += 1

	if status == DoomSim.QUIT:
		print("run_demo: quit after %d tics, at gametic %d" % [tics, sim.get_state()["gametic"]])
	else:
		printerr("run_demo: error after %d tics: %s" % [tics, sim.get_error()])
	sim.free()
	quit(0 if status == DoomSim.QUIT else 1)
