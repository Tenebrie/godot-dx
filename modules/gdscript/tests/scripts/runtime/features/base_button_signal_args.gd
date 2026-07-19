# BaseButton activation signals carry (mouse_button_index, event), but both have
# defaults so manual emission stays compatible, and handlers may declare fewer
# parameters (trailing arguments are dropped).
# Uses TextureButton: text-rendering controls (Button, Label, ...) cannot be
# instantiated in this test harness when SceneTree-fixture tests ran earlier.

func test():
	var b := TextureButton.new()

	b.pressed.connect(func (): print("no args"))
	b.pressed.connect(func (index): print("index ", index))
	b.pressed.connect(func (index, event): print("index ", index, " event ", event))

	b.pressed.emit()
	b.pressed.emit(MOUSE_BUTTON_RIGHT)
	b.pressed.emit(MOUSE_BUTTON_LEFT, null)

	b.toggled.emit(true)

	b.free()
