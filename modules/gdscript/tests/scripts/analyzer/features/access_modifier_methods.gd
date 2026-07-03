class Service:
	@private func _compute() -> int:
		return 21

	@protected func _scale(v: int) -> int:
		return v * 2

	func run() -> int:
		return _scale(_compute())

class Extended extends Service:
	@protected func _scale(v: int) -> int: # Overriding a protected method is allowed.
		return v * 3

	func run_extended() -> int:
		return _scale(10) # A protected method is accessible from a derived class.

func test():
	var service := Service.new()
	print(service.run())
	var extended := Extended.new()
	print(extended.run_extended())
