func test():
	var prices: Dictionary[String, int] = { "apple": 3 }
	var wrong_key: int = 1
	var wrong_value: String = "seven"
	prices.set(wrong_key, 2)
	prices.set("pear", wrong_value)
	var _pear: int = prices.get_or_add("pear", wrong_value)
	var _apple: int = prices.get(wrong_key)
	var _erased: bool = prices.erase(wrong_key)
	var _found: bool = prices.has(wrong_key)
	var _key: String = prices.find_key(wrong_value)
