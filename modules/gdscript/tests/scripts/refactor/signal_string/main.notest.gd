extends RefCounted

signal hit(damage: int)

func fire() -> void:
	hit.emit(1)
	hit.connect(on_hit)
	call("hit")

func on_hit(damage: int) -> void:
	pass
