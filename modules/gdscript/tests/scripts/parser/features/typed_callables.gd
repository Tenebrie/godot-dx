func _add(a: int, b: int) -> int:
	return a + b


func _greet(who: String) -> String:
	return "hi " + who


func _noop() -> void:
	pass


func test():
	# Basic declaration with signature; assigned from a method reference.
	var add: func(int, int) -> int = _add
	print(add.call(2, 3))

	# Single-arg, non-int return.
	var greet: func(String) -> String = _greet
	print(greet.call("alice"))

	# Zero args, void return.
	var noop: func() -> void = _noop
	noop.call()
	print("noop_ok")

	# `-> void` may be omitted.
	var noop_implicit: func() = _noop
	noop_implicit.call()
	print("noop_implicit_ok")

	# Typed callable is assignable to plain Callable (widening).
	var untyped: Callable = add
	print(untyped.call(10, 20))

	# Return type propagates: assigning to `int` works because .call() is typed.
	var sum: int = add.call(7, 8)
	print(sum)
