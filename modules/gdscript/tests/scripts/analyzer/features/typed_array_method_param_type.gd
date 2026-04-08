# Test that typed array method parameters accept the correct element type.

class Inner:
	var value: int
	func _init(v: int) -> void:
		value = v

func test():
	var arr: Array[int] = [1, 2, 3]

	# append / push_back should accept the element type.
	arr.append(4)
	arr.push_back(5)
	print(arr)

	# insert should accept the element type at a position.
	arr.insert(0, 0)
	print(arr)

	# fill should accept the element type.
	var fill_arr: Array[String] = ["", "", ""]
	fill_arr.fill("x")
	print(fill_arr)

	# Object element type.
	var obj_arr: Array[Inner] = []
	obj_arr.append(Inner.new(42))
	obj_arr.push_back(Inner.new(99))
	print(obj_arr[0].value)
	print(obj_arr[1].value)
