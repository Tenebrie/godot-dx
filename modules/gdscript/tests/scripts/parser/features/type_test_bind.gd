class Animal:
	var name := "animal"


class Dog extends Animal:
	var breed := "unknown"


class Cat extends Animal:
	var indoor := true


func describe(a: Variant) -> String:
	if a is Dog dog:
		return "dog " + dog.breed
	elif a is Cat cat:
		return "cat indoor=" + str(cat.indoor)
	elif a is Animal animal:
		return "animal " + animal.name
	return "unknown"


func test():
	var d := Dog.new()
	d.breed = "corgi"

	var c := Cat.new()
	c.indoor = false

	var a := Animal.new()
	a.name = "generic"

	print(describe(d))
	print(describe(c))
	print(describe(a))
	print(describe(42))

	# `and` chain: bind is available for the whole true branch.
	var x: Variant = d
	if x != null and x is Dog dog2:
		print("dog2 breed=" + dog2.breed)

	# Two binds in an `and` chain.
	var y: Variant = c
	if x is Dog dog3 and y is Cat cat3:
		print("both " + dog3.breed + " " + str(cat3.indoor))

	# Nested `if`.
	if x is Animal outer:
		if outer is Dog inner:
			print("nested " + inner.breed)

	# Bind is a constant reference (like match binds); ensure member access works.
	if x is Dog only_read:
		print("only_read.breed=" + only_read.breed)
