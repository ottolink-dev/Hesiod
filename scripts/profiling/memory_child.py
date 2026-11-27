#!/usr/bin/env python3
"""Private low-memory supervisor for memory_study.py (stdlib only).

Linux wait4 peaks retain RSS inherited at fork even across exec. Starting this
small interpreter first prevents the controller's catalog/results memory from
becoming an artificial workload peak. A small supervisor RSS floor remains and
is recorded explicitly; this tool is intended for larger native workloads.
"""
import json
import os
from pathlib import Path
import resource
import signal
import subprocess
import sys
import time


def process_group_rss_kib(group):
    total = 0
    for entry in Path('/proc').glob('[0-9]*'):
        try:
            fields = (entry / 'stat').read_text().rsplit(')', 1)[1].split()
            if int(fields[2]) == group:
                for line in (entry / 'status').read_text().splitlines():
                    if line.startswith('VmRSS:'):
                        total += int(line.split()[1])
        except (OSError, ValueError, IndexError):
            pass
    return total


def resident_kib():
    for line in Path('/proc/self/status').read_text().splitlines():
        if line.startswith('VmRSS:'):
            return int(line.split()[1])
    raise ValueError('cannot read supervisor RSS')


def child_limits():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def kill_group(pid):
    # Exiting between wait4(WNOHANG), a guard sample and killpg is harmless.
    try:
        os.killpg(pid, signal.SIGKILL)
    except ProcessLookupError:
        pass


def measure_direct(command, folder, env, timeout, max_rss_mib):
    """Reap one direct process with wait4; never use cumulative RUSAGE_CHILDREN.

    A killed/failed child still has a peak, but must never enter success medians.
    /proc process-group RSS sampling is an approximate safety limit, not the
    measured peak or a hard resource guarantee. Use a cgroup for a hard cap.
    """
    started = time.monotonic()
    reason = None
    supervisor_rss = resident_kib()
    with (folder / 'app.log').open('w') as log:
        child = subprocess.Popen(command, cwd=folder, env=env, stdout=log,
                                 stderr=subprocess.STDOUT, start_new_session=True,
                                 preexec_fn=child_limits)
        try:
            (folder / 'child.pid').write_text(str(child.pid))
            while True:
                pid, status, usage = os.wait4(child.pid, os.WNOHANG)
                if pid:
                    child.returncode = os.waitstatus_to_exitcode(status)
                    break
                if time.monotonic() - started >= timeout:
                    reason = 'timeout'
                    break
                if process_group_rss_kib(child.pid) > max_rss_mib * 1024:
                    reason = 'rss_limit'
                    break
                time.sleep(0.05)
        except BaseException:
            kill_group(child.pid)
            try:
                _, status, _ = os.wait4(child.pid, 0)
                child.returncode = os.waitstatus_to_exitcode(status)
            except ChildProcessError:
                pass  # An asynchronous signal can arrive just after reaping.
            raise
        if reason:
            # SIGKILL ensures an unattended native hang cannot ignore cleanup.
            kill_group(child.pid)
            _, status, usage = os.wait4(child.pid, 0)
            child.returncode = os.waitstatus_to_exitcode(status)
        # Kill any helpers still living in this otherwise completed job's group.
        kill_group(child.pid)
    return {'app_exit': child.returncode, 'failure_kind': reason,
            'supervisor_rss_before_spawn_kib': supervisor_rss,
            'process_peak_rss_kib': usage.ru_maxrss,
            'process_user_seconds': usage.ru_utime,
            'process_system_seconds': usage.ru_stime,
            'wall_seconds': time.monotonic() - started}



def main():
    request_path = Path(sys.argv[1]).resolve()
    request = json.loads(request_path.read_text())
    folder = request_path.parent
    result = measure_direct(request['command'], folder, dict(os.environ),
                            request['timeout'], request['max_rss_mib'])
    temporary = folder / 'measurement.json.tmp'
    temporary.write_text(json.dumps(result, indent=2) + '\n')
    temporary.replace(folder / 'measurement.json')


if __name__ == '__main__':
    def request_stop(signum, frame):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, request_stop)
    main()
