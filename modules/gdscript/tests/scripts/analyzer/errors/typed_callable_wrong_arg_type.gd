func _add(a: int, b: int) -> int:
	return a + b


func test():
	var add: func(int, int) -> int = _add
	print(add.call("hello", 2))
