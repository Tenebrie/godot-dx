# `:=` from a weak but known type propagates the type weakly instead of erroring.

var member_untyped = 1
var member_inferred := member_untyped

func check(param_untyped = 1, param_inferred := param_untyped):
	print(param_inferred)

func test():
	var local_untyped = 1
	var local_inferred := local_untyped
	print(member_inferred)
	print(local_inferred)
	check()
