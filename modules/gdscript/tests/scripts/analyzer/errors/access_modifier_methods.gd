class PrivateHolder:
	@private func _priv():
		pass

class ProtectedHolder:
	@protected func _prot():
		pass

class Unrelated:
	func use_private(holder: PrivateHolder):
		holder._priv()

	func use_protected(holder: ProtectedHolder):
		holder._prot()

class BadModifiers:
	@private @protected func _both():
		pass

func test():
	pass
