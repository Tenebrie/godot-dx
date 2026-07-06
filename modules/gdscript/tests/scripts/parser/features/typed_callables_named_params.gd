# Named parameters inside `func(...)` type annotations are allowed and
# treated as documentation — only the types are used for compatibility.

func sort_points(comparator: func(a: Vector3, b: Vector3) -> bool) -> bool:
	return comparator.call(Vector3.ZERO, Vector3.ONE)


func test():
	# Bare-type form still works.
	var f1: func(Vector3, Vector3) -> bool = func(a, b): return a.x < b.x
	print(f1.call(Vector3.ZERO, Vector3.ONE))

	# Named-parameter form works too.
	var f2: func(a: Vector3, b: Vector3) -> bool = func(a, b): return a.x < b.x
	print(f2.call(Vector3.ZERO, Vector3.ONE))

	# Passed as a function parameter.
	print(sort_points(func(a, b): return a.x < b.x))
