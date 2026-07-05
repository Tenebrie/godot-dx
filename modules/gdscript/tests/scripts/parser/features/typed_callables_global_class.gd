# Regression test: typed callables must compare their global-class parameter
# types correctly. Previously, `func(TypedCallableGlobalActor) -> int` was
# storing the arg as a resolved script-path (SCRIPT DataType) but the .call()
# argument was still a CLASS DataType with the class name — the two didn't
# compare equal, producing spurious "argument should be 'res://.../X.gd' but
# is 'X'" errors.

func _threat(actor: TypedCallableGlobalActor) -> int:
	return actor.actor_name.length()


func test():
	# Case A path — typed callable inferred from a fully typed lambda.
	var selector := func(actor: TypedCallableGlobalActor) -> int:
		return actor.actor_name.length()
	var a := TypedCallableGlobalActor.new()
	a.actor_name = "alice"
	print(selector.call(a))

	# Explicit annotation path — typed callable with declared func-type.
	var selector2: func(TypedCallableGlobalActor) -> int = _threat
	print(selector2.call(a))

	# Case B path — untyped lambda param inferred from LHS typed callable.
	var selector3: func(TypedCallableGlobalActor) -> int = func(actor):
		return actor.actor_name.length() * 2
	print(selector3.call(a))
