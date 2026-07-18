func test():
	var numbers: Array[int] = [1, 2, 3]
	var needle: String = "two"
	var _a: int = numbers.bsearch(needle)
	var _b: int = numbers.bsearch_custom(needle, func (x: int, y: int) -> bool: return x < y)
