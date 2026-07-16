class Actor:
	var actor_name: String


func test():
	var cb: func(Actor) -> int = func(_a, _b, _c):
		return 0
	print(cb)
