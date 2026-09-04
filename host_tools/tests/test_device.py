from openflightcomputer.device import UsbCdcConnection, wait_for_flight_port
from openflightcomputer.models import SerialPort


class FakeSerial:
    def __init__(self, reads):
        self.reads = list(reads)
        self.writes = bytearray()
        self.timeout = None
        self.write_timeout = None
        self.closed = False

    def read(self, _size=1):
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
    connection.close()
    assert device.closed
