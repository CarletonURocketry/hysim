# hysim

This is a collection of simulations that are meant to emulate the hybrid control system on a desktop computer over any
network.

This will allow acceptance testing of the system by propulsion members without first deploying the system. This also
permits testing during development of application level logic that is built on top of the POSIX layer, such as TCP
socket connections and messages, FSM logic for arming states and actuators and telemetry display and logging.

## Simulations

- [Telemetry Client](./telem_client/README.md)
- [Control Client](./control_client/README.md)
- [Pad Server](./pad_server/README.md)

## Building

To build the simulations, run `make all` in the project directory.

## Usage

To figure out how to use each of the simulations, run the compiled binary to the simulation with the help flag: `-h`.
This will print the help text.

You must always run the Pad Server simulation and one of the client simulations in order to emulate a system or part of
a system.

In console window #1:

```console
$ pad/pad
```

In console window #2:

```console
$ control_client/control
```

## Running with Docker

If you have docker installed, you can run simulations within a docker container.

To build the image run `docker build . -t hysim`.

In order to be able to receive data over TCP/UDP sockets, we have to manually expose the port when creating the container. To create a container with the exposed port for telemetry data, run `docker run -p 50002:50002 hysim` 
