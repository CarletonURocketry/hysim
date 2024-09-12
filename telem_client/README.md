# Telemetry Client

This simulation is an emulator of a telemetry client. It connects to the telemetry channel of the pad control box and 
consumes telemetry messages.

In reality, a telemetry client may be a user interface, a logging daemon or even a bundled control and telemetry client
process which uses the telemetry to provide visual feedback of control commands.

In this case, the telemetry client is a simple logging client which logs the telemetry packets to the console in
plain-text.
