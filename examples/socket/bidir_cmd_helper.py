#!/usr/bin/python3

import sys
import subprocess
import shlex
import os

# netcat (openbsd)
NC_CMD = "nc"
NC_OPT = "-UN"

def connect_and_execute_bidirectional_command(socket_path, command):
    TIMEOUT_SEC = 600

    # Sanitize user input (command stays as a string, but avoid passing raw input directly)
    safe_cmd = " ".join(shlex.split(command))

    # Launch nc process (connect to the Unix domain socket)
    p_nc = subprocess.Popen(
        [NC_CMD, NC_OPT, socket_path],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        shell=False,
        text=False,  # binary transfer
    )

    # Launch the bidirectional command connected to nc
    p_bidir_cmd = subprocess.Popen(
        safe_cmd,                # keep string command for shell=True
        stdin=p_nc.stdout,
        stdout=p_nc.stdin,
        stderr=None,
        shell=True,
        text=False,
    )

    # Close unused pipe ends in the parent process to propagate SIGPIPE properly
    p_nc.stdout.close()
    p_nc.stdin.close()

    try:
        p_bidir_cmd.communicate(timeout=TIMEOUT_SEC)

    except subprocess.TimeoutExpired:
        # Ensure the command is killed on timeout
        p_bidir_cmd.kill()

    except Exception as e:
        # Print unexpected exceptions for debugging
        print(type(e), file=sys.stderr)

    # Check bidirectional command exit code
    if p_bidir_cmd.returncode != 0:
        print(f"command exited with {p_bidir_cmd.returncode}", file=sys.stderr)

    # Ensure nc is terminated regardless of exceptions
    try:
        p_nc.terminate()
        p_nc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        # Force kill if graceful shutdown fails
        p_nc.kill()
    except Exception as e:
        print(type(e), file=sys.stderr)

    # Read and report stderr of nc
    nc_stderr = p_nc.stderr.read()
    if nc_stderr:
        print("nc stderr:", nc_stderr.decode(errors='ignore'), file=sys.stderr)

    return p_bidir_cmd.returncode


def main():
    script = os.path.basename(__file__)
    usage = f"Usage: {script} <socket-path> <bidirectional-command>"
    example = f"Example: {script} /tmp/tio-socket0 \"sz -b -p sample.bin\""
    note = f"Note: Please run \"tio -S <socket-path> <your-serial-device>\" beforehand."

    if len(sys.argv) != 3:
        print(usage, file=sys.stderr)
        print(example, file=sys.stderr)
        print(note, file=sys.stderr)
        return 1

    socket_path = sys.argv[1]
    bidirectional_command = sys.argv[2]
    return connect_and_execute_bidirectional_command(socket_path, bidirectional_command)


if __name__ == '__main__':
    exit_code = main()
    sys.exit(exit_code)
