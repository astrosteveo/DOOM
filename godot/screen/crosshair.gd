extends Control
## A small cross around this control's position, which DoomScreen puts
## at the middle of the view, where shots go with free look. It scales
## with the window, as Doom's own screen does.

## In 400ths of the window's height: the gap in the middle, and each
## arm's length and width.
const GAP := 1.5
const ARM := 3.0
const WIDTH := 1.0
const COLOR := Color(1.0, 1.0, 1.0, 0.75)


func _ready() -> void:
	get_window().size_changed.connect(queue_redraw)


func _draw() -> void:
	var unit := get_window().size.y / 400.0
	var gap := GAP * unit
	var arm := ARM * unit
	var width := maxf(WIDTH * unit, 1.0)
	for direction: Vector2 in [Vector2.RIGHT, Vector2.LEFT, Vector2.DOWN, Vector2.UP]:
		draw_line(direction * gap, direction * (gap + arm), COLOR, width)
