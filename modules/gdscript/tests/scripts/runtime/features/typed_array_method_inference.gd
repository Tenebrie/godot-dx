# Test type inference for typed Array method return types and lambda parameters.

class MyObj:
	extends RefCounted
	var value: int
	func _init(v: int = 0) -> void:
		value = v

func test():
	var arr: Array[int] = [1, 2, 3, 4, 5]

	# filter() should return Array[int].
	var filtered := arr.filter(func(e): return e > 2)
	print(filtered.get_typed_builtin())
	print(filtered)

	# duplicate() should return Array[int].
	var duped := arr.duplicate()
	print(duped.get_typed_builtin())

	# slice() should return Array[int].
	var sliced := arr.slice(1, 3)
	print(sliced.get_typed_builtin())

	# front/back should be element type.
	var first := arr.front()
	var last := arr.back()
	print(first + last)

	# map() should return untyped Array (transforms type).
	var mapped := arr.map(func(e): return str(e))
	print(mapped)

	# Test with Object types.
	var objs: Array[MyObj] = [MyObj.new(10), MyObj.new(20), MyObj.new(30)]

	# filter with lambda param inferred as MyObj.
	var big := objs.filter(func(e): return e.value > 15)
	print(big.size())

	# any/all with inferred param type.
	var has_big := objs.any(func(e): return e.value > 25)
	print(has_big)
	var all_positive := objs.all(func(e): return e.value > 0)
	print(all_positive)

	# pick_random should be element type.
	var picked := objs.pick_random()
	print(picked is MyObj)

	# reduce with element param inference (second param).
	var total: int = arr.reduce(func(acc, e): return acc + e, 0)
	print(total)

	# sort_custom with both params inferred.
	var sorted: Array[int] = arr.duplicate()
	sorted.sort_custom(func(a, b): return a > b)
	print(sorted)
