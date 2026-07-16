extends RefCounted

const Telegraph := preload("res://parser/features/typed_callables_cross_file_target.notest.gd")

var Validators: Array[func(Telegraph) -> Variant] = []


func addValidator(filter: func(Telegraph) -> Variant):
	Validators.push_back(filter)
	return self


func make_telegraph() -> Telegraph:
	return Telegraph.new()
