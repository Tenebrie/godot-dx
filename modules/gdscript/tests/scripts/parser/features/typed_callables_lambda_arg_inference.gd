# A lambda literal passed as an argument gets its parameter types from the
# typed callable signature declared by the receiving parameter.
# The bodies below call type-specific members, so a missing inference would
# leave the parameters as Variant and produce UNSAFE_METHOD_ACCESS warnings.

class Actor:
	var actor_name: String

	func _init(p_name: String) -> void:
		actor_name = p_name

	func describe() -> String:
		return "actor " + actor_name


class Telegraph:
	var filters: Array[Callable] = []

	func add_target_filter(filter: func(Actor) -> bool) -> Telegraph:
		filters.push_back(filter)
		return self

	func matches(actor: Actor) -> bool:
		for f in filters:
			if not f.call(actor):
				return false
		return true


func _apply(s: String, f: func(String) -> int) -> int:
	return f.call(s)


func _apply_two(n: int, s: String, f: func(int, String) -> String) -> String:
	return f.call(n, s)


func _apply_untyped(s: String, f: Callable) -> Variant:
	return f.call(s)


func test():
	# Single parameter inferred as String.
	print(_apply("hello", func(s):
		return s.length()
	))

	# Parameters are inferred positionally.
	print(_apply_two(2, "ab", func(n, s):
		return s.repeat(n)
	))

	# An explicit annotation on the lambda is kept as-is.
	print(_apply("four", func(s: String):
		return s.length()
	))

	# A plain `Callable` parameter infers nothing; the lambda stays untyped.
	print(_apply_untyped("noop", func(_s):
		return 0
	))

	# Inference works through a call on an object, and the lambda body can use
	# the inferred type's own members.
	var telegraph := Telegraph.new().add_target_filter(func(actor):
		return actor.actor_name.begins_with("a")
	).add_target_filter(func(actor):
		return actor.describe().length() > 5
	)
	print(telegraph.matches(Actor.new("alice")))
	print(telegraph.matches(Actor.new("bob")))
