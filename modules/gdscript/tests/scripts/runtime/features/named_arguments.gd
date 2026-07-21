class Base:
	func describe(prefix: String = "p", suffix: String = "s") -> String:
		return prefix + "-" + suffix


class Derived extends Base:
	func describe(prefix: String = "p", suffix: String = "s") -> String:
		return super(suffix: "S2")


class WithCtor:
	var total: int

	func _init(first: int = 1, second: int = 2) -> void:
		total = first * 10 + second


var eval_order := []


func many_args(a: int, b: int, c: int = 100, d: int = 200, e: int = 300) -> String:
	return "a=%d b=%d c=%d d=%d e=%d" % [a, b, c, d, e]


func tracked(tag: String, value: int) -> int:
	eval_order.push_back(tag)
	return value


func test():
	print(many_args(b: 2, a: 1))
	print(many_args(1, 2, e: 5))
	print(many_args(1, 2, d: 4))
	print(many_args(1, e: 5, b: 2))

	print(many_args(b: tracked("first", 2), a: tracked("second", 1)))
	print(eval_order)

	print("a,b,c,d".split(",", maxsplit: 2))
	print(String.num(3.14159, decimals: 2))
	print(clamp(value: 5, min: 0, max: 3))

	var n := Node.new()
	n.set_name(name: "NamedNode")
	print(n.name)

	var child := Node.new()
	n.add_child(child, internal: Node.INTERNAL_MODE_DISABLED)
	print(n.get_child_count())
	n.free()

	print(WithCtor.new(second: 7, first: 3).total)
	print(Derived.new().describe())
