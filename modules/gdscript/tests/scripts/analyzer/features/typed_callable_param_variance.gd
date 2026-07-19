# Lambda parameters annotated with a subtype or supertype of the declared
# callback parameter type are accepted — only unrelated types are rejected.
# Subtype annotations are the common real-world pattern, e.g. sorting
# `get_children()` (an `Array[Node]`) with lambdas typed to the actual child
# class.

func test():
	var nodes: Array[Node] = [Node3D.new(), Node3D.new()]

	# Subtype-annotated parameters (Node3D inherits Node).
	nodes.sort_custom(func(a: Node3D, b: Node3D) -> bool: return a.position.x < b.position.x)
	print(nodes.size())

	# Supertype-annotated parameters (Node is a base of Node3D).
	var spatials: Array[Node3D] = []
	var found := spatials.filter(func(n: Node) -> bool: return n != null)
	print(found.size())

	for n in nodes:
		n.free()
