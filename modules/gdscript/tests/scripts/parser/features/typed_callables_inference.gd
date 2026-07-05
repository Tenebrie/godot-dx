# Case A: a fully signed lambda infers a typed callable for its holder.
# Case B: an untyped lambda gets its parameter types from the LHS type.

func _apply(f: func(int) -> int, x: int) -> int:
	return f.call(x)


func test():
	# Case A — variable's type inferred from the lambda's declared signature.
	# The result is a `func(int) -> int` so `.call()` returns int, not Variant.
	var doubler := func(x: int) -> int:
		return x * 2
	var result: int = doubler.call(21)
	print(result)

	# Case A end-to-end: pass an inferred typed callable directly to a typed slot.
	print(_apply(doubler, 10))

	# Case A with default args: `.call()` still accepts the omitted arg.
	var with_default := func(a: int, b: int = 100) -> int:
		return a + b
	print(with_default.call(5))
	print(with_default.call(5, 7))

	# Case B — untyped lambda param gets its type from the LHS.
	# The body reads `.length()` which only exists on String.
	var length: func(String) -> int = func(s):
		return s.length()
	print(length.call("hello"))

	# Case B with method call requiring the inferred type.
	var greeter: func(String) -> String = func(name):
		return "hi " + name
	print(greeter.call("world"))
