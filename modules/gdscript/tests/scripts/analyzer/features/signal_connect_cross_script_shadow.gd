# Regression test: a same-named signal declared in the CURRENT script must not
# shadow another object's signal during lambda-parameter inference.
#
# Connecting to `emitter.payload_signal` must resolve to the emitter's
# SignalConnectCrossScriptEmitter.Payload (which has `payload_value`), not this
# script's own same-named `Payload` (which does not). Previously the name-based
# member search could latch onto the local same-named signal and infer the wrong
# (local) class for the lambda parameter.

signal payload_signal(payload: Payload)

class Payload:
	var unrelated_value: int

func test():
	var emitter := SignalConnectCrossScriptEmitter.new()
	emitter.payload_signal.connect(func(payload):
		# Must be the emitter's Payload, not this script's Payload.
		print(payload.payload_value)
	)
	emitter.fire(7)

	# Use the local signal so it is not reported as unused.
	payload_signal.emit(Payload.new())
