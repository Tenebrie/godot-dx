class Animal:
	var name := "animal"


class Dog extends Animal:
	var breed := "unknown"


func describe(a: Variant) -> String:
	if a is not Dog dog:
		return "not a dog"
	return "dog " + dog.breed


func plus_five(v: Variant) -> int:
	if v is not int n:
		return -1
	return n + 5


func test():
	var d := Dog.new()
	d.breed = "corgi"

	print(describe(d))
	print(describe(42))
	print(plus_five(1))
	print(plus_five("hi"))

	# Bind visible in outer scope after simple early return.
	var v: Variant = "hi!"
	if v is not String s:
		return
	print(s)

	# `else` branch is on the fall-through side too, so the bind is visible there.
	var q: Variant = "yo"
	if q is not String qs:
		return
	else:
		print("else " + qs)
	print("after " + qs)

	# OR chain: fall-through only when BOTH negated tests fail → both binds valid.
	var x: Variant = 3
	var y: Variant = 4
	if x is not int a or y is not int b:
		return
	print("both " + str(a + b))

	# Bind reflects the narrowed type — member access on bind works.
	var pet: Variant = d
	if pet is not Dog got_dog:
		return
	print(got_dog.breed)
