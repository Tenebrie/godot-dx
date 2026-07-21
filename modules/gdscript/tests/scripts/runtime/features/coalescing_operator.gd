# Coalescing operator (`??`): keeps the left value when it is truthy
# (evaluating it exactly once), otherwise evaluates and yields the right value.

var eval_count := 0

func count_and_return(value: Variant) -> Variant:
	eval_count += 1
	return value

func expensive_default() -> String:
	eval_count += 100
	return "default"

func test():
	# Truthy left keeps left.
	print("hello" ?? "fallback")
	print(42 ?? 0)
	# Falsy left falls back.
	print(null ?? "fallback")
	print("" ?? "empty fallback")
	print(0 ?? 7)
	print(false ?? true)

	# Left side evaluated exactly once; right side not evaluated when left is truthy.
	eval_count = 0
	var kept = count_and_return("kept") ?? expensive_default()
	print(kept)
	print(eval_count)

	# Right side evaluated when left is falsy.
	eval_count = 0
	var fallen = count_and_return(null) ?? expensive_default()
	print(fallen)
	print(eval_count)

	# Left-associative chaining takes the first truthy value.
	print(null ?? "" ?? "third")

	# Same hard types on both sides keep the type.
	var a: int = 1
	var b: int = 2
	var same := a ?? b
	var typed: int = same
	print(typed)
