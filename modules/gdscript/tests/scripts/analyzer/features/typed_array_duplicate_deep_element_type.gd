# Test that `duplicate_deep()` keeps the element type, so elements popped
# from the copy are typed too.

func test():
	var words: Array[String] = ["alpha", "beta"]
	var dup := words.duplicate_deep()
	var popped := dup.pop_back()
	print(popped.to_upper())
	print(dup)
