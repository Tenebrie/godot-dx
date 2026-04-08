# Test that a dot at the start of a line continues the previous line.

func get_array() -> Array[int]:
	return [1, 2, 3]

func test():
	# Dot continuation on method chains.
	var result := get_array()
		.duplicate()
		.size()
	print(result)

	# Dot continuation on string methods.
	var upper := "hello"
		.to_upper()
		.replace("L", "X")
	print(upper)

	# Dot continuation on array literal.
	var arr_size := [1, 2, 3]
		.size()
	print(arr_size)
