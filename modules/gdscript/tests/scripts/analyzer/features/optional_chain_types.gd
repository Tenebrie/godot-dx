# Optional-chain results keep their static type: object types stay unchanged
# (object variables already admit null), non-object types become weak claims
# of the real type — never Variant.

class Inner:
	var value: int = 42

	func get_value() -> int:
		return value

	func get_self() -> Inner:
		return self

func test():
	var obj := Inner.new()

	# Object results keep the hard object type.
	var same: Inner = obj?.get_self()
	print(same.value)

	# Typing survives through a chain of optional links.
	var chained: Inner = obj?.get_self()?.get_self()
	print(chained.value)

	# Non-object results are weak claims of the real type: `:=` infers them...
	var count := obj?.get_value()
	# ...and the value is usable as its claimed type without casts.
	var doubled: int = count * 2
	print(doubled)

	# Optional subscript keeps typed-container element typing.
	var arr: Array[String] = ["a", "b"]
	var first := arr?[0]
	print(first.to_upper())
