extends Node

# An untyped lambda assigned to a typed-callable target takes its parameter types
# from that target's signature. This covers assignments made separately from the
# declaration, including targets whose typed-callable type was inferred from a
# fully typed lambda rather than annotated.
# The bodies call String members, so a missing inference would leave the
# parameters as Variant and produce UNSAFE_METHOD_ACCESS warnings.

class Actor:
	var actor_name: String = "alice"


class Holder:
	var annotated: func(Actor) -> int
	var inferred = func(_a: Actor) -> int:
		return 0


@export var exported: func(Actor) -> int


func test():
	var h := Holder.new()

	# Annotated member.
	h.annotated = func(actor):
		return actor.actor_name.length()
	print(h.annotated.call(Actor.new()))

	# Annotated local, assigned separately from its declaration.
	var local: func(Actor) -> int
	local = func(actor):
		return actor.actor_name.length() * 2
	print(local.call(Actor.new()))

	# Exported member.
	exported = func(actor):
		return actor.actor_name.length() * 3
	print(exported.call(Actor.new()))

	# Local whose typed-callable type was inferred from a typed lambda.
	var soft = func(_a: Actor) -> int:
		return 0
	soft = func(actor):
		return actor.actor_name.length() * 4
	print(soft.call(Actor.new()))

	# Member whose typed-callable type was inferred from a typed lambda.
	h.inferred = func(actor):
		return actor.actor_name.length() * 5
	print(h.inferred.call(Actor.new()))

	# An explicit annotation on the lambda is kept as-is.
	h.annotated = func(actor: Actor):
		return actor.actor_name.length() * 6
	print(h.annotated.call(Actor.new()))
