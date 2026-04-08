# Test that nested collection types are properly supported.

func test():
	# Nested Array types.
	var nested: Array[Array[int]] = [[1, 2], [3, 4]]
	var val: int = nested[0][0]
	print(val)

	# Triple nesting.
	var deep: Array[Array[Array[String]]] = [[["a", "b"], ["c"]], [["d"]]]
	var s: String = deep[0][0][1]
	print(s)

	# Nested size.
	print(nested.size())
