# Verifies that `func(...)` types can be nested — a callable that takes another
# callable as an argument and/or returns a callable.

func _apply(f: func(int) -> int, x: int) -> int:
	return f.call(x)


func _make_adder(n: int) -> func(int) -> int:
	return func(x: int) -> int:
		return x + n


func test():
	# Higher-order call: pass a typed callable as an argument.
	var double: func(int) -> int = func(x: int) -> int:
		return x * 2
	print(_apply(double, 21))

	# Function returning a function.
	var add_5: func(int) -> int = _make_adder(5)
	print(add_5.call(10))

	# Explicit type on the higher-order type itself.
	var apply_type: func(func(int) -> int, int) -> int = _apply
	print(apply_type.call(double, 7))
