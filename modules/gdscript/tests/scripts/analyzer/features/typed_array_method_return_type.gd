# Test that typed array methods return the correct element type.

func test():
	var int_array: Array[int] = [1, 2, 3]

	# Methods that return a single element should return the element type.
	var front_val: int = int_array.front()
	var back_val: int = int_array.back()

	print(front_val)
	print(back_val)

	# Methods that return arrays should return typed arrays.
	var string_array: Array[String] = ["a", "b", "c", "a"]
	var filtered: Array[String] = string_array.filter(func(s: String) -> bool: return s != "a")
	var sliced: Array[String] = string_array.slice(1, 3)
	var duplicated: Array[String] = string_array.duplicate()

	print(filtered)
	print(sliced)
	print(duplicated)
