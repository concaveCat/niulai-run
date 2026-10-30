#!/usr/bin/env python3
"""Stream servo-zero ESP_LOG output and send its ASCII CLI over TCP.

The ESP32 isolated test firmware creates a local SoftAP and listens on
192.168.4.1:3333 by default. This tool deliberately sends ``stop`` when the
interactive session exits, unless ``--no-stop-on-exit`` is selected.
"""

from __future__ import annotations

import argparse
import socket
import sys
import threading
import time


def receive_loop(sock: socket.socket, stop: threading.Event, output) -> None:
    while not stop.is_set():
        try:
            data = sock.recv(4096)
        except socket.timeout:
            continue
        except OSError:
            return
        if not data:
            print("\n[ESP32 disconnected]", file=sys.stderr)
            return
        text = data.decode("utf-8", errors="replace")
        print(text, end="", flush=True)
        if output is not None:
            output.write(text)
            output.flush()


def send_command(sock: socket.socket, command: str) -> None:
    command = command.strip()
    if command:
        sock.sendall((command + "\n").encode("utf-8"))


def receive_until(
    sock: socket.socket,
    marker: str,
    timeout: float,
    output,
    receive_buffer: bytearray | None = None,
) -> bool:
    """Print and record the TCP stream until a marker appears.

    TCP is a byte stream: one recv can contain several command responses, and
    a marker can also be split across recv calls. Keep the bytes after the
    marker for the next operation instead of dropping them.
    """
    needle = marker.encode("utf-8")
    if receive_buffer is None:
        receive_buffer = bytearray()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        marker_index = receive_buffer.find(needle)
        if marker_index >= 0:
            del receive_buffer[:marker_index + len(needle)]
            return True
        try:
            data = sock.recv(4096)
        except socket.timeout:
            continue
        except OSError:
            return False
        if not data:
            return False
        receive_buffer.extend(data)
        text = data.decode("utf-8", errors="replace")
        print(text, end="", flush=True)
        if output is not None:
            output.write(text)
            output.flush()
    marker_index = receive_buffer.find(needle)
    if marker_index >= 0:
        del receive_buffer[:marker_index + len(needle)]
        return True
    print(f"\n[timeout waiting for {marker}]", file=sys.stderr)
    return False


def receive_command_until(
    sock: socket.socket,
    marker: str,
    timeout: float,
    output,
    receive_buffer: bytearray,
) -> bool:
    """Wait for a command marker and consume that command's end marker."""
    if not receive_until(sock, marker, timeout, output, receive_buffer):
        return False
    if marker == "TCP_DIAG_COMMAND_END":
        return True
    return receive_until(
        sock, "TCP_DIAG_COMMAND_END", timeout, output, receive_buffer
    )


def run_benchmark(sock: socket.socket, args: argparse.Namespace, output) -> int:
    """Run bounded pose rounds and preserve every diagnostic line."""
    if args.command:
        print("--benchmark cannot be combined with --command", file=sys.stderr)
        return 2

    receive_buffer = bytearray()
    if not receive_until(sock, "TCP_DIAG_CONNECTED", 5.0, output,
                         receive_buffer):
        return 1

    # Establish a known safe state and capture the initial channel targets.
    send_command(sock, "stop")
    if not receive_command_until(sock, "UART stop -> /OE HIGH", 3.0,
                                 output, receive_buffer):
        return 1
    send_command(sock, "list")
    if not receive_command_until(sock, "TCP_DIAG_COMMAND_END", 3.0,
                                 output, receive_buffer):
        return 1
    send_command(sock, "stats reset")
    if not receive_command_until(sock, "I2C stats reset", 3.0, output,
                                 receive_buffer):
        return 1

    command = f"pose pseudo {args.benchmark_amplitude}"
    for round_number in range(1, args.benchmark + 1):
        print(
            f"\n[BENCHMARK] round {round_number}/{args.benchmark}: {command}",
            flush=True,
        )
        send_command(sock, command)
        if not receive_command_until(
            sock,
            "rest pose reached; holding target; /OE LOW",
            args.benchmark_timeout,
            output,
            receive_buffer,
        ):
            # Stop immediately after a missing completion marker. The raw log
            # already contains the I2C failure or timeout diagnostic, if any.
            send_command(sock, "stop")
            receive_command_until(sock, "UART stop -> /OE HIGH", 3.0,
                                  output, receive_buffer)
            return 1
        send_command(sock, "stop")
        if not receive_command_until(sock, "UART stop -> /OE HIGH", 3.0,
                                     output, receive_buffer):
            return 1

    send_command(sock, "stats")
    if not receive_command_until(sock, "I2C stats:", 3.0, output,
                                 receive_buffer):
        return 1
    print("\n[BENCHMARK] completed", flush=True)
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="ESP32 servo-zero Wi-Fi TCP diagnostic console",
    )
    parser.add_argument("--host", default="192.168.4.1")
    parser.add_argument("--port", type=int, default=3333)
    parser.add_argument(
        "--command",
        action="append",
        help="send a command after connecting; may be repeated",
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=0.0,
        help="non-interactive stream duration in seconds; 0 means until Ctrl-C",
    )
    parser.add_argument(
        "--benchmark",
        type=int,
        metavar="ROUNDS",
        help="run bounded pose pseudo rounds and print final I2C diagnostics",
    )
    parser.add_argument(
        "--benchmark-amplitude",
        type=int,
        default=35,
        metavar="PCT",
        help="pose pseudo amplitude for --benchmark (default: 35)",
    )
    parser.add_argument(
        "--benchmark-timeout",
        type=float,
        default=8.0,
        metavar="SECONDS",
        help="completion timeout per benchmark round (default: 8)",
    )
    parser.add_argument("--output", help="also append received text to this file")
    parser.add_argument(
        "--no-stop-on-exit",
        action="store_true",
        help="do not send stop when the console exits",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    output = open(args.output, "a", encoding="utf-8") if args.output else None
    stop = threading.Event()
    try:
        with socket.create_connection((args.host, args.port), timeout=8.0) as sock:
            sock.settimeout(0.5)
            if args.benchmark is not None:
                if args.benchmark <= 0 or not 0 <= args.benchmark_amplitude <= 100:
                    print("benchmark rounds must be >0 and amplitude must be 0..100", file=sys.stderr)
                    return 2
                return run_benchmark(sock, args, output)

            receiver = threading.Thread(
                target=receive_loop,
                args=(sock, stop, output),
                daemon=True,
            )
            receiver.start()
            for command in args.command or []:
                send_command(sock, command)
                time.sleep(0.05)

            if args.command or args.duration > 0.0:
                deadline = time.monotonic() + args.duration if args.duration > 0 else None
                while receiver.is_alive() and (deadline is None or time.monotonic() < deadline):
                    time.sleep(0.1)
            else:
                print("Commands are sent as ASCII lines. Ctrl-C exits and sends stop.")
                while receiver.is_alive():
                    try:
                        command = input("diag> ")
                    except EOFError:
                        break
                    send_command(sock, command)
    except KeyboardInterrupt:
        pass
    except OSError as error:
        print(f"connection failed: {error}", file=sys.stderr)
        return 2
    finally:
        stop.set()
        if output is not None:
            output.close()

    if not args.no_stop_on_exit:
        try:
            with socket.create_connection((args.host, args.port), timeout=1.0) as sock:
                send_command(sock, "stop")
        except OSError:
            pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
