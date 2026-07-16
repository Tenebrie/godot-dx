# A lambda literal takes its parameter types from the typed callable it is being
# stored into, whichever construct does the storing.
# The bodies call String members, so a missing inference would leave the
# parameters as Variant and produce UNSAFE_METHOD_ACCESS warnings.

class Actor:
	var actor_name: String = "alice"


func _make() -> func(Actor) -> int:
	return func(a):
		return a.actor_name.length()


func _take(cb: func(Actor) -> int) -> int:
	return cb.call(Actor.new())


func test():
	# Returned from a function declared to return a typed callable.
	print(_make().call(Actor.new()))

	# Selected by a ternary.
	var picked: func(Actor) -> int = (func(a):
		return a.actor_name.length() * 2) if true else (func(a):
		return a.actor_name.length() * 3)
	print(picked.call(Actor.new()))

	# A ternary passed as an argument.
	print(_take((func(a):
		return a.actor_name.length() * 4) if false else (func(a):
		return a.actor_name.length() * 5)))

	# Element of a typed Array literal.
	var arr: Array[func(Actor) -> int] = [func(a):
		return a.actor_name.length() * 6
	]
	print(arr[0].call(Actor.new()))

	# Value of a typed Dictionary literal.
	var d: Dictionary[String, func(Actor) -> int] = {"x": func(a):
		return a.actor_name.length() * 7
	}
	print(d["x"].call(Actor.new()))

	# Appended to a typed Array (the element type drives the parameter).
	var arr2: Array[func(Actor) -> int] = []
	arr2.append(func(a):
		return a.actor_name.length() * 8
	)
	print(arr2[0].call(Actor.new()))
