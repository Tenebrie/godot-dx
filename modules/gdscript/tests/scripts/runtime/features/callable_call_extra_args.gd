func plain_fn(a: int) -> void:
	print("plain_fn ", a)

func two_arg_fn(a: int, b: String) -> void:
	print("two_arg_fn ", a, " ", b)

func test():
	var typed_lambda := func(x: int) -> void: print("typed_lambda ", x)
	typed_lambda.call(1)
	typed_lambda.call(2, true)
	typed_lambda.call(3, "extra", 4.5)

	var untyped_lambda := func(x): print("untyped_lambda ", x)
	untyped_lambda.call(10)
	untyped_lambda.call(20, "extra")

	var method_callable: Callable = plain_fn
	method_callable.call(100)
	method_callable.call(200, true, "extra")

	var two_arg_callable: Callable = two_arg_fn
	two_arg_callable.call(1, "one")
	two_arg_callable.call(2, "two", 42, null)
