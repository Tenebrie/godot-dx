func _add(a: int, b: int) -> int:
	return a + b


func test():
	var add: func(int, int) -> int = _add
	# .call() returns int; assigning the result to a String should fail.
	var s: String = add.call(1, 2)
	print(s)
