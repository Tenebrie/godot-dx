class Actor:
	var actor_name: String


func _wrong(_a: int, _b: int) -> String:
	return "x"


func test():
	var cb: func(Actor) -> int = _wrong
	print(cb)
