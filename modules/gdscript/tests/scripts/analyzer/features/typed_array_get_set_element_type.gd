# `Array.get()` returns the element type and `Array.set()` takes it.

func test():
	var words: Array[String] = ["a", "b"]
	var got := words.get(1)
	print(got.to_upper())
	words.set(0, "c")
	print(words)
