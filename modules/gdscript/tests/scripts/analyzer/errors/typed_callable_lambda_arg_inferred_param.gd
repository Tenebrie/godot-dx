func _apply(s: String, f: func(String) -> int) -> int:
	return f.call(s)


func test():
	print(_apply("hello", func(s):
		return s.no_such_method()
	))
