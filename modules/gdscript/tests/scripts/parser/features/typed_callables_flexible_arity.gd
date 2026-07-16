# A callable that accepts fewer arguments than its typed-callable target passes is
# valid, because extra arguments are discarded at call time. Parameters beyond the
# target's arity are also fine when they have defaults.

class Actor:
	var actor_name: String = "alice"


func _exact(a: Actor) -> int:
	return a.actor_name.length()


func _fewer() -> int:
	return 2


func _defaulted(a: Actor, b: int = 5) -> int:
	return a.actor_name.length() + b


func test():
	var exact: func(Actor) -> int = _exact
	print(exact.call(Actor.new()))

	var fewer: func(Actor) -> int = _fewer
	print(fewer.call(Actor.new()))

	var defaulted: func(Actor) -> int = _defaulted
	print(defaulted.call(Actor.new()))

	# A lambda may also accept fewer parameters than the target passes.
	var lambda_fewer: func(Actor) -> int = func():
		return 9
	print(lambda_fewer.call(Actor.new()))
