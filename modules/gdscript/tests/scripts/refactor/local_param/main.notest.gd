extends RefCounted

var amount := 5

func damage(amount: int) -> int:
	return amount + 1

func heal(amount: int) -> int:
	return amount + 2
