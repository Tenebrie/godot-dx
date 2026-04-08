signal my_signal(value: int, name: String)

func test():
	my_signal.emit("not_an_int", 42)
