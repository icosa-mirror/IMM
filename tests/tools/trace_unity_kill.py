"""Trace SIGKILL senders on an isolated CI runner without recording arguments or secrets."""
import argparse
import json
import os
from pathlib import Path
import re
import selectors
import signal
import subprocess
import time


def trace(output, duration):
    stopping = False

    def stop(*_):
        nonlocal stopping
        stopping = True

    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, stop)
    root = Path('/sys/kernel/tracing')
    instance = root / 'instances' / f'imm-unity-kill-{os.getpid()}'
    enabled = []
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('a', buffering=1) as stream:
        def emit(kind, **values):
            stream.write(f'{json.dumps(dict(time=time.time(), kind=kind, **values))}\n')

        try:
            if not (root / 'events').exists():
                subprocess.run(['mount', '-t', 'tracefs', 'tracefs', str(root)],
                               capture_output=True, check=True, timeout=10)
            instance.mkdir()
            for option in ('record-tgid', 'userstacktrace'):
                path = instance / 'options' / option
                if path.exists():
                    path.write_text('1')
            event = instance / 'events/signal/signal_generate'
            (event / 'filter').write_text('sig == 9')
            (event / 'enable').write_text('1')
            enabled.append(event)
            for name in ('sys_enter_kill', 'sys_enter_tkill', 'sys_enter_tgkill'):
                syscall = instance / 'events/syscalls' / name
                if syscall.exists():
                    (syscall / 'filter').write_text('sig == 9')
                    (syscall / 'enable').write_text('1')
                    enabled.append(syscall)
            exec_event = instance / 'events/sched/sched_process_exec'
            if exec_event.exists():
                (exec_event / 'enable').write_text('1')
                enabled.append(exec_event)
            oom = instance / 'events/oom/mark_victim'
            if oom.exists():
                (oom / 'enable').write_text('1')
                enabled.append(oom)
            emit('ready', kernel=os.uname().release,
                 events=[str(p.relative_to(instance)) for p in enabled],
                 signal_format=(event / 'format').read_text())
            fd = os.open(instance / 'trace_pipe', os.O_RDONLY | os.O_NONBLOCK)
            try:
                with selectors.DefaultSelector() as selector:
                    selector.register(fd, selectors.EVENT_READ)
                    # Verify signal visibility with one child owned by this tracer.
                    # Its known PID distinguishes this deliberate kill from failures.
                    probe = subprocess.Popen(['sleep', '30'])
                    emit('self_test', sender_pid=os.getpid(), target_pid=probe.pid)
                    try:
                        os.kill(probe.pid, signal.SIGKILL)
                    finally:
                        probe.wait(timeout=5)
                    probe_observed = False
                    deadline = time.monotonic() + duration
                    pending = b''
                    while not stopping and time.monotonic() < deadline:
                        if not selector.select(timeout=0.25):
                            continue
                        try:
                            pending += os.read(fd, 65536)
                        except BlockingIOError:
                            continue
                        while b'\n' in pending:
                            line, pending = pending.split(b'\n', 1)
                            # Only these enabled tracepoints enter this private instance.
                            # Prefix identifies sender task/PID; payload identifies target.
                            decoded = line.decode(errors='replace')
                            emit('kernel_event', trace=decoded)
                            if 'sched_process_exec:' in decoded:
                                match = re.search(r'\bpid=(\d+)\b', decoded)
                                if match:
                                    proc = Path('/proc') / match[1]
                                    try:
                                        exe = (proc / 'exe').resolve(strict=True)
                                        if exe.name == 'Unity':
                                            status = (proc / 'status').read_text().splitlines()
                                            args = (proc / 'cmdline').read_bytes().split(b'\0')
                                            # Classify activation without retaining any argument values.
                                            role = 'activation' if b'-serial' in args else 'return_license' if b'-returnlicense' in args else 'project'
                                            emit('unity_process', pid=int(match[1]), role=role,
                                                 identity=[s for s in status if s.split(':', 1)[0] in ('Name', 'Pid', 'Tgid', 'PPid', 'NSpid')],
                                                 executable=str(exe),
                                                 executable_maps=[s for s in (proc / 'maps').read_text().splitlines() if 'r-xp' in s])
                                    except OSError:
                                        pass  # A short-lived exec may already have exited.
                            if 'signal_generate:' in decoded and re.search(rf'\bpid={probe.pid}\b', decoded):
                                probe_observed = True
                    emit('self_test_result', observed=probe_observed)
            finally:
                os.close(fd)
        except (OSError, subprocess.SubprocessError) as error:
            emit('unavailable', error=f'{type(error).__name__}: {error}')
        finally:
            for event in enabled:
                try:
                    (event / 'enable').write_text('0')
                except OSError:
                    pass
            if instance.exists():
                try:
                    instance.rmdir()
                except OSError as error:
                    emit('cleanup_error', error=str(error))
            emit('stop')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--max-seconds', type=int, default=2700)
    args = parser.parse_args()
    trace(args.output, args.max_seconds)
