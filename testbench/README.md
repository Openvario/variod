# Test bench for variod

`variod` relies on `sensord` to supply with vario input. Instead of running a
real `sensord` daemon on real hardware, a simulator can be used. `variod`
generates vario sound based on input from `sensord` and forwards NMEA traffic
to XCSoar. So, there are total 3 processes involved in a typical operating
environment:

* `sensord` - connects to variod on TCP port 4353
* `variod` - listens TCP port 4353 and forwards traffic to port 4352
* `xcsoar` - listens on TCP port 4352

Traffic flow can be described in diagram:

```
+---------+         +--------+        +--------+
|         |         |        +-------->        |
| sensord +---------> variod |        | xcsoar |
|         |         |        <--------+        |
+---------+         +--------+        +--------+
```

This project provides tools for testing variod daemon by simulating other parts
of the equation (`sensord` and `xcsoar`)

## Setting up environment

Development environment is managed by `pipenv`, so you need to install it
first:

```
pip3 install pipenv
```

Then development environment can be initialized by running

```
pipenv install
```

## Running

Before running any command, set up your shell with:

```
pipenv shell
```

After that, the following commands should become available:

* `sensord-sim` - simulator for real sensor daemon

### Quick start

Run thesse commands in 3 separate terminals:

| Terminal 1    | Terminal 2     | Terminal 3             |
|---------------|----------------|------------------------|
| `nc -lp 4352` | `../variod -f` | `sensord-sim generate` |

### Simulating sensord

`sensord-sim` replaces `sensord` process. Sensord suports several modes via command line options.

`generate` mode generates NMEA stream that goes from minimum vario reading to
maximum and back periodically. For example:

```
seonsord-sim generate --min=2 --max=5 --period=10
```

`replay` mode simply forwards NMEA stream from pre-recorded file

```
seonsord-sim replay samples/sample_flight_2.nmea
```

It also allows to configure the delay between each sentence sent using
`--delay` flag.  For example, to stress-test variod, one can flood it with NMEA
messages by setting the delay to 0:

```
sensord-sim --delay=0 replay samples/sample_flight_2.nmea
```

See `sensord-sim --help` for the full list of options.

### Simulating xcsoar

The most primitive way to simulate `xcsoar` process is to listen for port 4352
using netcat command:

```
nc -lp 4352
```

One can also run the actual xcsoar command and configure the openvario device
on port 4352.