from openflightcomputer.device import UsbCdcConnection, wait_for_flight_port
from openflightcomputer.models import SerialPort


class FakeSerial:
    def __init__(self, reads):
        self.reads = list(reads)
        self.read_sizes = []
        self.writes = bytearray()
        self.timeout = None
        self.write_timeout = None
        self.closed = False

    def read(self, size=1):
        self.read_sizes.append(size)
        return self.reads.pop(0) if self.reads else b""

    def write(self, data):
        chunk = bytes(data[:2])
        self.writes.extend(chunk)
        return len(chunk)

    def close(self):
        self.closed = True


def test_wait_selects_flight_identity():
    ports = (
        SerialPort("other", 0xCAFE, 0x4001),
        SerialPort("flight", 0xCAFE, 0x4002),
    )
    assert wait_for_flight_port(port_lister=lambda: ports).device == "flight"


def test_connection_frames_partial_io_and_crlf():
    device = FakeSerial([b'{"type":', b'"log"}\r\nnext\n'])
    connection = UsbCdcConnection(SerialPort("fake", 0xCAFE, 0x4002), device)
    connection.write_line(b"hello")
    assert bytes(device.writes) == b"hello\n"
    assert connection.read_line() == b'{"type":"log"}'
    assert connection.read_line() == b"next"
    assert all(size == 1 for size in device.read_sizes)
    connection.close()
    assert device.closed


def test_connection_drains_exactly_the_bytes_already_waiting():
    class BufferedFakeSerial(FakeSerial):
        @property
        def in_waiting(self):
            return len(self.reads[0]) if self.reads else 0

    device = BufferedFakeSerial([b'{"type":"log"}\n'])
    connection = UsbCdcConnection(SerialPort("fake", 0xCAFE, 0x4002), device)

    assert connection.read_line() == b'{"type":"log"}'
    assert device.read_sizes == [15]
