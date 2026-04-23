#!/usr/bin/env python3
# Streams QEMU serial output line by line, terminates QEMU as soon as every
# expected marker has been seen, and fails cleanly on a wall-clock timeout.
#
# Why not just ``execute_process(... TIMEOUT ...)`` in CMake: that model always
# runs for the full timeout before the log can be checked, which means either
# fast local runs waste time or slow CI runs miss markers. Streaming + early
# exit keeps local iteration fast AND gives CI generous headroom.

import argparse
import os
import signal
import subprocess
import sys
import threading
import time


def parse_args():
    parser = argparse.ArgumentParser(description="Run QEMU and match serial markers with early exit.")
    parser.add_argument("--log", required=True, help="Path to write captured serial output.")
    parser.add_argument("--timeout", type=float, required=True, help="Wall-clock ceiling in seconds.")
    parser.add_argument("--marker", action="append", default=[],
                        help="Required marker substring (repeatable, order-independent).")
    parser.add_argument("qemu_cmd", nargs=argparse.REMAINDER,
                        help="Separator '--' followed by the QEMU command to run.")
    args = parser.parse_args()
    cmd = args.qemu_cmd
    if cmd and cmd[0] == "--":
        cmd = cmd[1:]
    if not cmd:
        parser.error("missing QEMU command after '--'")
    return args, cmd


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
    args, cmd = parse_args()
    markers = list(args.marker)
    seen = set()
    output_lines = []
    done = threading.Event()

    with open(args.log, "w", encoding="utf-8", errors="replace") as log_file:
        proc = subprocess.Popen(
            cmd,
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
                    for marker in markers:
                        if marker not in seen and marker in line:
                            seen.add(marker)
                    if len(seen) == len(markers):
                        done.set()
                        return
            finally:
                done.set()

        reader_thread = threading.Thread(target=reader, daemon=True)
        reader_thread.start()

        deadline = time.monotonic() + args.timeout
        while not done.is_set():
            remaining = deadline - time.monotonic()
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

    elapsed = args.timeout - max(0.0, deadline - time.monotonic())
    missing = [marker for marker in markers if marker not in seen]

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

    sys.stdout.write(f"Smoke run matched all {len(markers)} markers in {elapsed:.2f}s.\n")
    sys.exit(0)


if __name__ == "__main__":
    main()
