from io import StringIO
from types import SimpleNamespace
import unittest

from tools.servo_zero_tcp_console import receive_until, run_benchmark, send_command


class FakeSocket:
    def __init__(self):
        self.sent = []

    def sendall(self, data):
        self.sent.append(data)


class FakeBenchmarkSocket:
    def __init__(self, coalesce_responses=False):
        self.sent = []
        self.coalesce_responses = coalesce_responses
        self.responses = [b"TCP_DIAG_CONNECTED; send ASCII commands ending LF\n"]

    def append_response(self, body):
        if self.coalesce_responses:
            self.responses.append(body + b"TCP_DIAG_COMMAND_END\n")
        else:
            if body:
                self.responses.append(body)
            self.responses.append(b"TCP_DIAG_COMMAND_END\n")

    def sendall(self, data):
        self.sent.append(data)
        command = data.decode("ascii").strip()
        if command == "stop":
            self.append_response(b"UART stop -> /OE HIGH\n")
        elif command == "list":
            self.append_response(b"")
        elif command == "stats reset":
            self.append_response(b"I2C stats reset\n")
        elif command.startswith("pose pseudo"):
            self.append_response(
                b"rest pose reached; holding target; /OE LOW\n"
            )
        elif command == "stats":
            self.append_response(
                b"I2C stats: logical=4 attempts=5 nack=1\n"
            )

    def recv(self, _size):
        if self.responses:
            return self.responses.pop(0)
        raise TimeoutError


class SingleResponseSocket:
    def __init__(self):
        self.read = False

    def recv(self, _size):
        if self.read:
            raise TimeoutError
        self.read = True
        return b"FIRST SECOND"


class ServoZeroTcpConsoleTests(unittest.TestCase):
    def test_commands_are_ascii_lines(self):
        sock = FakeSocket()

        send_command(sock, "stats")
        send_command(sock, "  stop  ")
        send_command(sock, "")

        self.assertEqual(sock.sent, [b"stats\n", b"stop\n"])

    def test_benchmark_sends_bounded_rounds_and_final_stats(self):
        sock = FakeBenchmarkSocket()
        output = StringIO()
        args = SimpleNamespace(
            command=None,
            benchmark=2,
            benchmark_amplitude=35,
            benchmark_timeout=1.0,
        )

        result = run_benchmark(sock, args, output)

        self.assertEqual(result, 0)
        self.assertEqual(
            sock.sent,
            [
                b"stop\n",
                b"list\n",
                b"stats reset\n",
                b"pose pseudo 35\n",
                b"stop\n",
                b"pose pseudo 35\n",
                b"stop\n",
                b"stats\n",
            ],
        )
        self.assertIn("I2C stats: logical=4 attempts=5 nack=1", output.getvalue())

    def test_benchmark_handles_coalesced_command_responses(self):
        sock = FakeBenchmarkSocket(coalesce_responses=True)
        output = StringIO()
        args = SimpleNamespace(
            command=None,
            benchmark=1,
            benchmark_amplitude=35,
            benchmark_timeout=1.0,
        )

        self.assertEqual(run_benchmark(sock, args, output), 0)
        self.assertIn("UART stop -> /OE HIGH", output.getvalue())
        self.assertIn("rest pose reached; holding target; /OE LOW", output.getvalue())
        self.assertIn("I2C stats: logical=4 attempts=5 nack=1", output.getvalue())

    def test_receive_until_preserves_bytes_after_marker(self):
        sock = SingleResponseSocket()
        receive_buffer = bytearray()
        output = StringIO()

        self.assertTrue(receive_until(sock, "FIRST", 0.1, output,
                                      receive_buffer))
        self.assertTrue(receive_until(sock, "SECOND", 0.1, output,
                                      receive_buffer))


if __name__ == "__main__":
    unittest.main()
