# Regression test: connecting a lambda to a signal declared on ANOTHER script
# must infer the lambda parameter as the signal's exact declared type, even when
# that type is an inner (nested) class.
#
# The cross-script path used to fail to resolve the signal's owning class, so it
# fell back to reconstructing the parameter type from the signal's PropertyInfo.
# An inner class cannot be represented there, so it collapsed to its native base
# ("RefCounted"), producing a spurious UNSAFE_PROPERTY_ACCESS warning and losing
# the real type for autocomplete/type checking.

func test():
	var emitter := SignalConnectCrossScriptEmitter.new()
	emitter.payload_signal.connect(func(payload):
		# `payload` must be inferred as SignalConnectCrossScriptEmitter.Payload,
		# so `payload_value` resolves without an unsafe-access warning.
		print(payload.payload_value)
	)
	emitter.fire(42)
