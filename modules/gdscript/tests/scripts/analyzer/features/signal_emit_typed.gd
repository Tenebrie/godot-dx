# Test that signal.emit() validates argument types against signal declaration.

signal my_signal(value: int, name: String)
signal no_args()

func test():
	# Correct types should pass without error.
	my_signal.connect(func(value: int, name: String):
		print("value=%d name=%s" % [value, name])
	)
	my_signal.emit(42, "hello")

	# Signal with no args.
	no_args.connect(func():
		print("no_args emitted")
	)
	no_args.emit()
