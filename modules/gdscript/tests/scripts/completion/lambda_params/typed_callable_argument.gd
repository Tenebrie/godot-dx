class Actor:
	var actor_name: String

	func describe() -> String:
		return actor_name


func add_target_filter(filter: func(Actor) -> bool) -> void:
	pass


func _init() -> void:
	add_target_filter(func(actor):
		actor.➡
	)
