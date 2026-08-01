class StorageBox:
	var stored_value := 1

	func retrieve() -> int:
		return stored_value


func test():
	var snapshot: Variant = StorageBox.new()
	assert(snapshot is StorageBox storage)
	storage.➡
