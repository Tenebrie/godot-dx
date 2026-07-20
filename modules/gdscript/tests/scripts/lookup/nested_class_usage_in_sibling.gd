extends RefCounted

class InnerBase extends RefCounted:
	pass

class Consumer extends RefCounted:
	func make():
		var x := Inner➡Base.new()
		print(x)
