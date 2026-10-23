class_name DoomScreen
extends CanvasLayer
## Doom's own 2D screen over the 3D view: the status bar, messages, the
## menus, the automap, the intermission, the title pages and the
## player's weapon, drawn by the game as in the SDL3 build, with the 3D
## view left transparent where Doom would draw it. Over all of it, the
## palette flashes, red for damage, gold for a pickup and green for the
## radiation suit, as a tint of the colour Doom's palette blends toward.

@export var sim: DoomSim

## Where the 3D view shows, in the screen's pixels: empty when the screen
## covers it. The screen fills the window.
var view := Rect2i()
var size := Vector2i(1, 1)
## Pixels from the view's middle to where Doom sees 45 degrees aside.
var projection := 1.0

@onready var _picture: TextureRect = %Picture
@onready var _flash: ColorRect = %Flash

var _texture: ImageTexture
var _tints: Array[Color] = []


## Sizes the screen for the window, and keeps it so; call once the game
## has started.
func start() -> void:
	for palette in 14:
		_tints.append(sim.get_palette_tint(palette))
	get_window().size_changed.connect(_resize)
	_resize()


## Has the game draw its screen fraction of the way between tics, and
## shows it, tinted by palette. Returns what DoomSim.draw() returned.
func draw(fraction: float, palette: int) -> int:
	var status := sim.draw(fraction)
	if status != DoomSim.RUNNING:
		return status
	var screen := sim.get_screen()
	var image: Image = screen["image"]
	if _texture and Vector2i(_texture.get_size()) == image.get_size():
		_texture.update(image)
	else:
		_texture = ImageTexture.create_from_image(image)
		_picture.texture = _texture
	size = image.get_size()
	view = screen["view"]
	projection = screen["projection"]
	_flash.color = _tints[palette] if palette < _tints.size() else Color.TRANSPARENT
	return status


func _resize() -> void:
	sim.set_screen_size(get_window().size)
