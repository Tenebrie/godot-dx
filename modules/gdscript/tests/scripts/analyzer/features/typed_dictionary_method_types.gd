# Test that typed dictionary methods use the key and value types.

func test():
	var prices: Dictionary[String, int] = { "apple": 3, "banana": 5 }

	var apple := prices.get("apple")
	var pear := prices.get_or_add("pear", 7)
	var banana := prices.find_key(5)
	print(apple + 1)
	print(pear + 1)
	print(banana.to_upper())

	var key_list := prices.keys()
	var value_list := prices.values()
	var first_key := key_list.front()
	var first_value := value_list.front()
	print(first_key.to_upper())
	print(first_value + 1)

	var dup := prices.duplicate()
	var combined := prices.merged({ "cherry": 2 })
	var cherry := combined.get("cherry")
	print(dup.keys().size())
	print(cherry + 1)
