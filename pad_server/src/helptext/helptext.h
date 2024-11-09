#define HELP_TEXT                                                                                                      \
    "pad 0.0.0\n2024 CU InSpace\n\nDESCRIPTION:\n    Emulates the pad control box"                                     \
    " server.\n\nUSAGE:\n    pad [options]\n\nOPTIONS:\n    -f file     A CSV fil"                                     \
    "e containing sensor data telemetry to transmit. If not\n                spec"                                     \
    "ified, no sensor data telemetry will be sent.\n    -t port     The port numb"                                     \
    "er to use for the telemetry connection. If not\n                specified, p"                                     \
    "ort 50002 is used.\n    -a addr     The multicast address for the telemetry connection. If not\n                " \
    "specified, "                                                                                                      \
    "address "                                                                                                         \
    "224.0.0.10 is used.\n"                                                                                            \
    "    -c port     The port number to use for the controlle"                                                         \
    "r connection. If not\n                specified, port 50001 is used.\n\nEXAM"                                     \
    "PLES:\n    pad -t ../thecoldhasflown.csv\n"
