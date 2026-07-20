extends RefCounted

var health := 10

func damage(amount: int) -> void:
	health -= amount

func heal() -> void:
	var health := 0
	health += 1
	self.health = health + 2
