extends CanvasLayer
## Doom's own 2D screen over the 3D view: the status bar, messages, the
## menus, the automap, the intermission, the title pages and the
## player's weapon, drawn by the game as in the SDL3 build, with the 3D
## view left transparent where Doom would draw it. Over all of it, the
## palette flashes, red for damage, gold for a pickup and green for the
## radiation suit, as a tint of the colour Doom's palette blends toward.
## Under those, with free look, a crosshair where shots go.

const PALETTES := 14

@export var sim: DoomSim

## Where the 3D view shows, in the screen's pixels: empty when the screen
## covers it. The screen fills the window.
var view := Rect2i()
var size := Vector2i(1, 1)
## Pixels from the view's middle to where Doom sees 45 degrees aside.
var projection := 1.0

@onready var _picture: TextureRect = %Picture
@onready var _flash: ColorRect = %Flash
@onready var _crosshair: Control = %Crosshair

var _texture: ImageTexture


## Sizes the screen for the window, and keeps it so; call once the game
## has started.
func start() -> void:
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
	# Fetched each frame, not cached, so the flash follows the gamma the 2D layer does.
	_flash.color = sim.get_palette_tint(palette) if palette > 0 and palette < PALETTES else Color.TRANSPARENT
	return status


## Shows the crosshair at the middle of the view, which the camera looks
## through and the player shoots at with free look, or hides it. Call it
## after draw().
func show_crosshair(shown: bool) -> void:
	_crosshair.visible = shown and view.has_area()
	if _crosshair.visible:
		var rect := _picture.get_global_rect()
		_crosshair.position = rect.position + Vector2(view.get_center()) / Vector2(size) * rect.size


## Where a point in the window is on the screen, in the screen's pixels,
## as the menu takes the cursor.
func to_screen(point: Vector2) -> Vector2i:
	var rect := _picture.get_global_rect()
	return Vector2i(((point - rect.position) / rect.size * Vector2(size)).floor())


func _resize() -> void:
	sim.set_screen_size(get_window().size)
