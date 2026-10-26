#!/usr/bin/env python3
"""Record only the warmed HIGH stereo callback interval via perf's acked control FDs."""
import os
import pathlib
import subprocess
import sys

if len(sys.argv) != 4:
    raise SystemExit('record_perf.py /absolute/perf /absolute/step3-bench output.data')
perf, executable, output = sys.argv[1:]
control_read, control_write = os.pipe()
ack_read, ack_write = os.pipe()
try:
    command = [perf, 'record', '-e', 'cpu-clock:u', '-F', '499',
               '--call-graph', 'dwarf,16384', '--delay', '-1',
               '--control', f'fd:{control_read},{ack_write}',
               '-o', output, '--', executable, 'profile',
               str(control_write), str(ack_read)]
    print('Command:', command, flush=True)
    subprocess.run(command, pass_fds=(control_read, control_write, ack_read, ack_write), check=True)
finally:
    for descriptor in (control_read, control_write, ack_read, ack_write):
        os.close(descriptor)
