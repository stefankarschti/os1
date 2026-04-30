#!/usr/bin/env python3
# Streams QEMU serial output line by line, terminates QEMU as soon as every
# expected marker has been seen, and fails cleanly on a wall-clock timeout.
#
# Why not just ``execute_process(... TIMEOUT ...)`` in CMake: that model always
# runs for the full timeout before the log can be checked, which means either
# fast local runs waste time or slow CI runs miss markers. Streaming + early
# exit keeps local iteration fast AND gives CI generous headroom.

import argparse
from dataclasses import dataclass
import os
import signal
import socket
import subprocess
import sys
import threading
import time

@dataclass
class SendEvent:
    marker: str
    text: str
    sent: bool = False


@dataclass
class MonitorEvent:
    marker: str
    text: str
    sent: bool = False


def parse_event_spec(parser, spec, flag_name):
    marker, separator, text = spec.partition("::")
    if not separator:
        parser.error(f"invalid {flag_name} value '{spec}', expected MARKER::TEXT")
    return marker, bytes(text, "utf-8").decode("unicode_escape")


def send_monitor_text(socket_path, text):
    commands = [line for line in text.splitlines() if line]
    if not commands:
        commands = [text]

    for command in commands:
        deadline = time.monotonic() + 5.0
        last_error = None
        payload = command if command.endswith("\n") else (command + "\n")
        while time.monotonic() < deadline:
            try:
                with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
                    sock.connect(socket_path)
                    sock.settimeout(0.5)
                    try:
                        sock.recv(4096)
                    except socket.timeout:
                        pass
                    sock.sendall(payload.encode("utf-8"))
                    try:
                        sock.recv(4096)
                    except socket.timeout:
                        pass
                    break
            except OSError as exc:
                last_error = exc
                time.sleep(0.05)
        else:
            raise RuntimeError(f"failed to send monitor text to '{socket_path}': {last_error}")

        time.sleep(0.05)


def parse_args():
    parser = argparse.ArgumentParser(description="Run QEMU and match serial markers with early exit.")
    parser.add_argument("--log", required=True, help="Path to write captured serial output.")
    parser.add_argument("--timeout", type=float, required=True, help="Wall-clock ceiling in seconds.")
    parser.add_argument("--settle-after-markers", type=float, default=0.0,
                        help="Extra time to keep streaming after all required markers are seen.")
    parser.add_argument("--marker", action="append", default=[],
                        help="Required marker substring (repeatable, order-independent).")
    parser.add_argument("--reject-marker", action="append", default=[],
                        help="Forbidden marker substring (repeatable, fails immediately if seen).")
    parser.add_argument("--send-after", action="append", default=[],
                        help="Send text to QEMU stdin after seeing MARKER using MARKER::TEXT with \\n escapes.")
    parser.add_argument("--monitor-socket", default=None,
                        help="Optional UNIX socket path for a QEMU HMP monitor.")
    parser.add_argument("--monitor-send-after", action="append", default=[],
                        help="Send HMP text to the QEMU monitor after seeing MARKER using MARKER::TEXT with \\n escapes.")
    parser.add_argument("qemu_cmd", nargs=argparse.REMAINDER,
                        help="Separator '--' followed by the QEMU command to run.")
    args = parser.parse_args()
    cmd = args.qemu_cmd
    if cmd and cmd[0] == "--":
        cmd = cmd[1:]
    if not cmd:
        parser.error("missing QEMU command after '--'")
    send_events = []
    for spec in args.send_after:
        marker, text = parse_event_spec(parser, spec, "--send-after")
        send_events.append(SendEvent(marker=marker, text=text))
    monitor_events = []
    for spec in args.monitor_send_after:
        marker, text = parse_event_spec(parser, spec, "--monitor-send-after")
        monitor_events.append(MonitorEvent(marker=marker, text=text))
    return args, cmd, send_events, monitor_events


def terminate(proc):
    if proc.poll() is not None:
        return
    try:
        proc.terminate()
        proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        proc.kill()
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            pass
    except ProcessLookupError:
        pass


def main():
    args, cmd, send_events, monitor_events = parse_args()
    markers = list(args.marker)
    reject_markers = list(args.reject_marker)
    seen = set()
    rejected = None
    output_lines = []
    done = threading.Event()
    deadline = time.monotonic() + args.timeout
    settle_deadline = None

    with open(args.log, "w", encoding="utf-8", errors="replace") as log_file:
        proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            bufsize=1,
            text=True,
            errors="replace",
            # Put QEMU in its own process group so we can always kill it cleanly.
            start_new_session=True,
        )

        def reader():
            try:
                assert proc.stdout is not None
                for line in proc.stdout:
                    output_lines.append(line)
                    log_file.write(line)
                    log_file.flush()
                    for marker in reject_markers:
                        if marker in line:
                            nonlocal_rejected[0] = marker
                            done.set()
                            return
                    for marker in markers:
                        if marker not in seen and marker in line:
                            seen.add(marker)
                    if proc.stdin is not None:
                        for event in send_events:
                            if (not event.sent) and ((event.marker in seen) or (event.marker in line)):
                                try:
                                    proc.stdin.write(event.text)
                                    proc.stdin.flush()
                                except BrokenPipeError:
                                    pass
                                event.sent = True
                    if args.monitor_socket is not None:
                        for event in monitor_events:
                            if (not event.sent) and ((event.marker in seen) or (event.marker in line)):
                                try:
                                    send_monitor_text(args.monitor_socket, event.text)
                                except RuntimeError as exc:
                                    nonlocal_monitor_error[0] = str(exc)
                                    done.set()
                                    return
                                event.sent = True
                    if len(seen) == len(markers):
                        if args.settle_after_markers <= 0:
                            done.set()
                            return
                        if settle_deadline_ref[0] is None:
                            settle_deadline_ref[0] = time.monotonic() + args.settle_after_markers
                    if settle_deadline_ref[0] is not None and time.monotonic() >= settle_deadline_ref[0]:
                        done.set()
                        return
            finally:
                done.set()

        nonlocal_rejected = [None]
        nonlocal_monitor_error = [None]
        settle_deadline_ref = [None]
        reader_thread = threading.Thread(target=reader, daemon=True)
        reader_thread.start()

        while not done.is_set():
            current_deadline = settle_deadline_ref[0] if settle_deadline_ref[0] is not None else deadline
            remaining = current_deadline - time.monotonic()
            if remaining <= 0:
                break
            done.wait(timeout=min(0.2, remaining))

        terminate(proc)
        # Best-effort: also kill anything left in the child's process group.
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        except (ProcessLookupError, OSError):
            pass

        reader_thread.join(timeout=2)

    rejected = nonlocal_rejected[0]
    monitor_error = nonlocal_monitor_error[0]
    settle_deadline = settle_deadline_ref[0]
    elapsed = args.timeout - max(0.0, deadline - time.monotonic())
    if settle_deadline is not None and len(seen) == len(markers):
        elapsed = max(elapsed, settle_deadline - (deadline - args.timeout))
    missing = [marker for marker in markers if marker not in seen]

    if rejected is not None:
        sys.stderr.write(f"Smoke run hit forbidden marker '{rejected}'.\n")
        sys.stderr.write("--- captured serial log ---\n")
        sys.stderr.writelines(output_lines)
        if not output_lines or not output_lines[-1].endswith("\n"):
            sys.stderr.write("\n")
        sys.stderr.write(f"--- end log ({args.log}) ---\n")
        sys.exit(1)

    if monitor_error is not None:
        sys.stderr.write(f"Smoke run monitor command failed: {monitor_error}\n")
        sys.stderr.write("--- captured serial log ---\n")
        sys.stderr.writelines(output_lines)
        if not output_lines or not output_lines[-1].endswith("\n"):
            sys.stderr.write("\n")
        sys.stderr.write(f"--- end log ({args.log}) ---\n")
        sys.exit(1)

    if missing:
        sys.stderr.write(
            f"Smoke run reached {len(seen)}/{len(markers)} markers in {elapsed:.2f}s "
            f"(timeout={args.timeout:.1f}s). Missing:\n"
        )
        for marker in missing:
            sys.stderr.write(f"  - {marker}\n")
        sys.stderr.write("--- captured serial log ---\n")
        sys.stderr.writelines(output_lines)
        if not output_lines or not output_lines[-1].endswith("\n"):
            sys.stderr.write("\n")
        sys.stderr.write(f"--- end log ({args.log}) ---\n")
        sys.exit(1)

    if settle_deadline is not None:
        sys.stdout.write(
            f"Smoke run matched all {len(markers)} markers and stayed clean for "
            f"{args.settle_after_markers:.2f}s.\n"
        )
    else:
        sys.stdout.write(f"Smoke run matched all {len(markers)} markers in {elapsed:.2f}s.\n")
    sys.exit(0)


if __name__ == "__main__":
    main()
