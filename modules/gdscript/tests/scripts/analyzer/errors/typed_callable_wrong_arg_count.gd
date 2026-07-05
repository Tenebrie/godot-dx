func _add(a: int, b: int) -> int:
	return a + b


func test():
	var add: func(int, int) -> int = _add
	# Too few arguments.
	print(add.call(1))
	# Too many arguments.
	print(add.call(1, 2, 3))
