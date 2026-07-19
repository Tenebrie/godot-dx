# A lambda whose parameter types are inferred from the container (only the return
# type is written) still counts as a typed callable, so `map`/`reduce` results are
# typed from it. Asserted by the absence of INFERENCE_ON_VARIANT / unsafe warnings.

func test():
	var numbers: Array[int] = [1, 2, 3]

	var bools := numbers.map(func (n) -> bool: return n > 1)
	var popped_bool := bools.pop_back()
	print(popped_bool)

	var flag := numbers.reduce(func (stuff, n) -> bool: return bool(stuff) or n > 2)
	print(flag)
