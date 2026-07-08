class_name SignalConnectCrossScriptEmitter
extends RefCounted

# Inner (nested) class used as the signal's parameter type. An inner class has no
# faithful PropertyInfo representation, so recovering it requires the signal's
# declaration node rather than a PropertyInfo round-trip.
class Payload:
	var payload_value: int

	func _init(value: int) -> void:
		payload_value = value

signal payload_signal(payload: Payload)

func fire(value: int) -> void:
	payload_signal.emit(Payload.new(value))
