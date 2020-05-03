sensord_mock    initial release Mai 1, 2020 RN

Overview:
sensord_mock is a tool to generate NMEA sentences to be processed by variod and
XCSoar further down the stream.
By default sensord_mock sends on port 4352 .
sensord_mock is intended to replace sensord. It works on a Linux system.

sensord_mock can generate NMEA sentences with "random" content. Namely sentences
like this: >$POV,E,-0.01,P,957.29,Q,0.00*3F<.
Please consult the OpenVario documentation for the description of
the POV format.

The second mode of operation is to use a nmea log file to replay (a part of) a flight.
For this mode of operation it is important that the NMEA log has $GPRMC sentences.
It uses date and time information from the $GPRMC sentences to control the replay speed.
A simple way to generate a NMEA log is to turn on "Setup->Logger->NMEA_logger".

In the simplest form the tool is executed without any arguments: sensord_mock
It generates one $POV sentence every 500 ms.

The second mode of operation is to replay a NMEA log file:
sensord_mock -i sample_flight_1.nmea

There are a number of command line options:
-v : verbose
-x factor : time scale factor, 1 means at normal speed, 2 means twice as fast
        0.5 means at half the speed. Values range from 0.1 to 1001 .
-d : in replay mode the input file may contain "dirty" sentences. E.g. checksum
        failed. These sentences are dropped unless this flag is set in which
        case dirty sentences are passed sent out, too.
-l msec : insert a minimum latency of msec milliseconds after each NMEA sentence

Example:
sensord_mock -d -x 20 -v -i sample_flight_1.nmea
This will replay from the NMEA log "" at 20x the speed, in verbose mode, dirty NMEA
sentences will be sent.

Compile:
make

Using it:
Stop variod and sensord in case they are running as daemons.
Stop XCSoar in case it is running.
To see how it works open 3 windows on your Linux computer. After successful
compile "make" start netcat in the first window: "nc -l -p 4352".

In the second window start variod: "variod -d2 -f -c /opt/conf/variod.conf"

In the third window start sensord_mock: "sensord_mock -v"

You should see in the first window what variod would normally have sent
to XCSoar. If you understand what is going on and you are happy with it
then ^C in the first window to stop netcat (nc).
Instead start XCSoar and navigate to "Config->Devices". Configure and enable
one of the devices as "OpenVario on TCP port 4352". XCSoar will now listen
on port 4352 doing what netcat did in the previous step.
If all goes well, you can watch the vario needle moving, the IAS jumping around,
and the Baro Altitude jumping around.

You will hear the sound from variod if this experiment is running on a platform
which supports PulseAudio. Turn off vario sound in XCSoar "Gauges->Audio_Vario"
to make sure it's variod you get the sound from.

my_netcat:
It is intended for systems which don't provide "nc -l".
Run it instead of XCSoar: It reports what variod would have sent to XCSoar
if XCSoar was actually running. Instead start "my_netcat -p 4352".
It can be used to diff the output before and after a change to variod, for instance.

