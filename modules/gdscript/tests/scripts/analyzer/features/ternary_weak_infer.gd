# A ternary mixing hard and weak sources has a weak type; `:=` accepts it weakly.

func test():
	var left_hard_int := 1
	var right_weak_int = 2
	var result_hm_int := left_hard_int if true else right_weak_int
	print(result_hm_int)
