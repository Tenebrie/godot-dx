# `?.` guards only its own base: when the guard passes, the rest of the chain
# runs normally, so a null produced *after* a passed guard still errors on ".".
class Inner:
	var value: int = 0
	var next: Inner = null

func test():
	var a := Inner.new()
	print(a?.next.value)
