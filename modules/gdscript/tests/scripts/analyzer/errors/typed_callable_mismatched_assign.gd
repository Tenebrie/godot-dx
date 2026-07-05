func _add(a: int, b: int) -> int:
	return a + b


func test():
	var add: func(int, int) -> int = _add
	# Signature mismatch: assigning func(int, int) -> int to func(String) -> int.
	var wrong: func(String) -> int = add
	print(wrong)
