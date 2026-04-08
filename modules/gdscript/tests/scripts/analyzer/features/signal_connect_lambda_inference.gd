# Test that lambda parameters connected to signals infer their types.

signal my_signal(value: int, name: String)

func test():
	# Lambda connected to signal should infer param types.
	my_signal.connect(func(value, name):
		var v: int = value
		var n: String = name
		print("value=%d name=%s" % [v, n])
	)
	my_signal.emit(42, "hello")

	# Lambda with fewer params than signal declares should also work.
	my_signal.connect(func(value):
		var v: int = value
		print("partial=%d" % v)
	)
	my_signal.emit(99, "world")
