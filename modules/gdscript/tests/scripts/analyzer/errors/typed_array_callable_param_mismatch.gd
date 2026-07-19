func test():
	var nodes: Array[Node3D] = []
	var _result := nodes.filter(func (s: String) -> bool: return s.is_empty())
