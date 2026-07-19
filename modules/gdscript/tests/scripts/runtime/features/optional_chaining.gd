# Optional chaining (`?.` and `?[`): a null or freed base short-circuits the
# whole remaining postfix chain and yields null instead of erroring.

class Inner:
	var value: int = 42
	var next: Inner = null
	var calls: int = 0

	func get_value() -> int:
		return value

	func add(x: int) -> int:
		return value + x

	func poke() -> void:
		calls += 1

var bumps: int = 0

func bump() -> int:
	bumps += 1
	return 1

func test():
	var obj := Inner.new()
	var nothing: Inner = null

	# Valid base: behaves exactly like ".".
	print(obj?.value)
	print(obj?.get_value())

	# Null base: attribute access and calls short-circuit to null.
	print(nothing?.value)
	print(nothing?.get_value())

	# The call must not run on a null base, and its arguments must not be
	# evaluated either.
	obj?.poke()
	nothing?.poke()
	print(obj.calls)
	print(obj?.add(bump()))
	print(nothing?.add(bump()))
	print(bumps)

	# Full-chain short-circuit: everything after a tripped guard is skipped.
	print(nothing?.next.value)
	var mid := Inner.new()
	mid.next = Inner.new()
	mid.next.value = 7
	print(mid?.next.value)
	print(mid?.next?.value)
	print(mid?.next?.next?.value)
	print(mid.next?.value)

	# Optional subscript.
	var data = {"key": [1, 2, 3]}
	var no_data = null
	print(data?["key"])
	print(no_data?["key"])
	print(no_data?["key"].size())
	print(data?["key"]?[0])

	# Freed objects count as null (is_instance_valid semantics).
	var freed_node := Node.new()
	freed_node.free()
	print(freed_node?.get_child_count())

	# `:=` infers a weak type from optional-chain results, so null can still
	# flow through at runtime.
	var count := obj?.get_value()
	print(count)
	print(typeof(count) == TYPE_INT)
	var missing := nothing?.get_value()
	print(missing)
	print(typeof(missing) == TYPE_NIL)
