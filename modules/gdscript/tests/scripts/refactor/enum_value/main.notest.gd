extends RefCounted

enum State { IDLE, RUNNING }

var state := State.IDLE

func update() -> void:
	if state == State.IDLE:
		state = State.RUNNING
