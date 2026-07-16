const Def := preload("res://parser/features/typed_callables_cross_file_holder.notest.gd")

# This file has no dependency on the target script at all — it only knows Def. The lambda
# parameter types below are imported from Def's parser tree, so resolving members on them
# requires this parser to have cached the target's external parser.


func test():
	var d := Def.new()

	# Parameter type of a typed callable declared in another file.
	d.addValidator(func(telegraph):
		return telegraph.telegraph_name
	)
	print(d.Validators.size())

	# Element type of a typed-callable Array declared in another file.
	d.Validators.push_back(func(telegraph):
		return telegraph.describe()
	)
	print(d.Validators.size())

	var t := d.make_telegraph()
	print(d.Validators[0].call(t))
	print(d.Validators[1].call(t))
