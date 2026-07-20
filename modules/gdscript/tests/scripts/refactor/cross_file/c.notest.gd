extends RefCounted

const A = preload("res://refactor/cross_file/a.notest.gd")

func run() -> int:
	var a := A.new()
	var untyped = null
	untyped = a
	return a.do_thing() + untyped.do_thing()
