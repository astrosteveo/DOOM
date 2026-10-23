class_name DoomSpace
## Doom's map space in Godot's. Doom's x is east, y north and z up, in
## map units; Godot's y is up and -z is north. A map unit is 1/32 m, so
## the player, 56 units tall, is 1.75 m.

const SCALE := 1.0 / 32.0


## A point in map units.
static func point(x: float, y: float, z: float) -> Vector3:
	return Vector3(x, z, -y) * SCALE


## A yaw for a Godot node facing the way a Doom angle points: Doom's
## angles are counterclockwise from east, and a Godot node faces -z.
static func yaw(angle: float) -> float:
	return angle - PI / 2.0
