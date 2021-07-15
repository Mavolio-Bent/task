# Task

## Dependencies:

Ensure that you have following dependencies installed:
`Paho MQTT C++`
`lipqxx`
`Boost C++`

## Build

```
$ git clone https://github.com/Mavolio-Bent/task.git
$ cd task
$ make ARGS=BOOST_ROOT && make install
```
where `BOOST_ROOT` is installation path of Boost. Binaries are then located in `/out` directory.

## Usage
To launch server run
```
$ cd task/out
$ server <host> <port> <mqtt-broker address>
```
To launch dbservice run
```
$ cd task/out
$ dbserver <dbuser> <password> <address> <port> <mqtt-broker address>
```
Ensure that MQTT Broker is active