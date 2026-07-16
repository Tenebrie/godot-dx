class Actor:
	var actor_name: String


func test():
	var cb: func(Actor) -> int = func(_a):
		return "not an int"
	print(cb)
