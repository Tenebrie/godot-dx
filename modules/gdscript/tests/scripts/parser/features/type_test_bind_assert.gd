class Animal:
	var name := "animal"


class Dog extends Animal:
	var breed := "unknown"


func describe(a: Variant) -> String:
	assert(a is Animal animal)
	return animal.name


func test():
	var d := Dog.new()
	d.breed = "corgi"

	var x: Variant = d
	assert(x is Dog dog)
	print("breed=" + dog.breed)

	# `and` chain: both binds are available after the assert.
	var y: Variant = Animal.new()
	assert(x is Dog dog2 and y is Animal animal2)
	print(dog2.breed + " " + animal2.name)

	# With a custom message.
	assert(x is Animal named, "expected an animal")
	print(named.name)

	print(describe(d))
