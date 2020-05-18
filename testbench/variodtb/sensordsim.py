from typing import Generator

import logging
import math
import time
import socket
import argparse

log = logging.getLogger(__name__)

VARIOD_HOST = "localhost"
VARIOD_PORT = 4353


class NmeaGenerator:
    """Abstract interface for NMEA generators."""

    def generate(self) -> Generator[str, None, None]:
        """Generate NMEA sentence (with the checksum)."""
        raise NotImplementedError()


class FileGenerator(NmeaGenerator):
    """Generate arbitrary sentences by reading from the file."""

    def __init__(self, fname: str) -> None:
        """Create file generator

        fname: file name to read.
        """
        self.fname = fname

    def generate(self) -> Generator[str, None, None]:
        while True:
            with open(self.fname, "r") as f:
                yield from f


class VarioSinGenerator(NmeaGenerator):
    """Synthesize sensord variometer readings.

    Iteratively go from minimum to maximum vario reading during given period
    of time.
    """

    def __init__(self, start: float, end: float, period: float) -> None:
        """Create NMEA generator.

        start: minimum vario value
        end: maximum vario value
        period: time in seconds to go from start to the end and back
        """
        self.start = start
        self.end = end
        self.period = period

    def generate(self) -> Generator[str, None, None]:
        vario_gen = self.generate_vario()
        while True:
            vario = next(vario_gen)
            sentence = f"$POV,E,{vario:.2f}"
            yield add_nmea_chksum(sentence) + "\r\n"

    def generate_vario(self) -> Generator[float, None, None]:
        rng = self.end - self.start

        while True:
            t = time.time()
            vario = math.sin(t * 2 * math.pi / self.period)
            # sin() generates values from -1 to 1. Make it from 0 to 1
            vario = (vario + 1) / 2
            # Apply the dynamic range
            vario = vario * rng + self.start
            yield vario


class SensordSim:
    """Sensor daemon simulator."""

    def __init__(self, nmea_gen: NmeaGenerator, delay: float) -> None:
        """Create simulator

        nmea_gen: NMEA sentence generator to use
        delay: delay between sentences in seconds
        """
        self.nmea_gen = nmea_gen
        self.delay = delay

    def run(self) -> None:
        """Start sensor simulation.

        Never finishes.
        """
        log.info("Starting sensord-sim")

        while True:
            sock = self._wait_for_server()
            with sock:
                try:
                    self._simulate(sock)
                except ConnectionError as e:
                    log.warning(f"Connection closed: {e}")

    def _wait_for_server(self) -> socket.socket:  # pylint: disable=no-self-use
        log.info(f"Connecting to {VARIOD_HOST}:{VARIOD_PORT}...")
        while True:
            try:
                sock = socket.create_connection((VARIOD_HOST, VARIOD_PORT), 1)
                log.info(f"Connection established")
                return sock
            except ConnectionRefusedError:
                time.sleep(1)
        return sock

    def _simulate(self, sock: socket.socket):
        nmea_gen = self.nmea_gen.generate()
        while True:
            nmea = next(nmea_gen)
            log.info(f">>> {nmea.strip()}")
            sock.send(nmea.encode())
            if self.delay:
                time.sleep(self.delay)


def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Sensor daemon simulator.")
    parser.add_argument(
        "--delay",
        "-d",
        type=float,
        default=0.1,
        metavar="SECONDS",
        help="Delay between every NMEA sentence (default: 0.1s)",
    )

    subparsers = parser.add_subparsers(dest="gen", help="sub-command help")

    replay_parser = subparsers.add_parser("replay", help="Replay .nmea file")
    replay_parser.add_argument("file", help="Path to .nmea file to replay")

    replay_parser = subparsers.add_parser("generate", help="Sinthesize vario readings")
    replay_parser.add_argument(
        "--min",
        type=float,
        default=-2.0,
        help="Minimum vario reading (default: -2 m/s)",
    )
    replay_parser.add_argument(
        "--max", type=float, default=5.0, help="Maximum vario reading (default: 5 m/s)"
    )
    replay_parser.add_argument(
        "--period",
        type=float,
        default=10.0,
        help=(
            "Time in seconds to go from minimum vario reading to maximum "
            "and back (default: 10s)"
        ),
    )
    return parser


def add_nmea_chksum(sentence: str) -> str:
    checksum = 0
    for c in sentence:
        checksum ^= ord(c)
    return f"{sentence}*{checksum:02X}"


def _pick_generator(args):
    if args.gen == "replay":
        return FileGenerator(args.file)

    assert args.gen == "generate"
    return VarioSinGenerator(args.min, args.max, args.period)


def main() -> None:
    parser = make_parser()
    args = parser.parse_args()
    FORMAT = "%(asctime)-15s %(message)s"
    logging.basicConfig(level=logging.INFO, format=FORMAT)

    gen = _pick_generator(args)
    sim = SensordSim(gen, args.delay)
    try:
        sim.run()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
