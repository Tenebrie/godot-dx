func test():
	var nodes: Array[Node3D] = []
	nodes.fil➡ter(func (n: Node3D) -> bool: return n.visible)
