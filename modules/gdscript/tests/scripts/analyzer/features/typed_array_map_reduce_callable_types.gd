# Test callable-taking typed array methods: callback parameters take the element
# type (asserted by the absence of unsafe warnings), `map` results are weakly
# element-typed, and `reduce` with a compatible accumulator is hard-typed.

func test():
	var words: Array[String] = ["a", "bb"]

	# Callback parameter `w` is inferred as String — `w.length()` would warn otherwise.
	var found := words.find_custom(func (w): return w.length() > 1)
	print(found)
	var rfound := words.rfind_custom(func (w): return w.length() > 1)
	print(rfound)

	# `reduce` with typed callable and int accumulator returns hard int.
	var numbers: Array[int] = [1, 2, 3]
	var total := numbers.reduce(func (accum: int, n: int) -> int: return accum + n, 0)
	print(total + 1)

	# `reduce` without an accumulator is weakly typed from the callable.
	var weak_total := numbers.reduce(func (accum: int, n: int) -> int: return accum + n)
	print(weak_total + 1)

	# `bsearch_custom` comparator parameters are element-typed.
	var index := numbers.bsearch_custom(2, func (a, b): return a < b)
	print(index)

	# `map` results are weakly typed `Array[T]` from the callable's return type:
	# elements flow through pop/front with their type (no unsafe warning on
	# `to_upper()`), while the runtime array stays untyped.
	var strings := numbers.map(func (n: int) -> String: return str(n))
	var popped := strings.pop_back()
	print(popped.to_upper())
	print(strings)
