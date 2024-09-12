# Control Client

This simulation emulates a control input box in the system, where all actuator and arming commands originate.

A limitation of this simulation is that it currently only implements a pure controller. That is, it does not maintain
state about the current arming level. It also does not receive any part of the telemetry stream to gain information
about the pad's current arming state. It purely sends commands and displays the response from the pad server.
