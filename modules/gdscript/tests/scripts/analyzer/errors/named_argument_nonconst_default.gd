var base_value := 10


func my_func(alpha: int, beta: int = base_value, gamma: int = 2) -> int:
	return alpha + beta + gamma


func test():
	my_func(1, gamma: 5)
