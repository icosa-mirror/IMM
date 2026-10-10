"""Record live Unity Docker memory evidence without collecting environment variables."""
import argparse
import json
import os
from pathlib import Path
import queue
import signal
import subprocess
import threading
import time


def counters(text):
    return {key: int(value) for key, value in (line.split() for line in text.splitlines())}


def cgroup_directory(text, root=Path('/sys/fs/cgroup')):
    for line in text.splitlines():
        if line.startswith('0::'):
            candidate = (root / line[3:].lstrip('/')).resolve()
            if candidate.is_relative_to(root.resolve()):
                return candidate
    return None


def read_memory(directory):
    result = {}
    for name in ('memory.current', 'memory.peak', 'memory.max', 'memory.swap.current', 'memory.events'):
        try:
            value = (directory / name).read_text().strip()
            result[name] = counters(value) if name == 'memory.events' else value
        except OSError:
            pass
    return result


def docker(*args):
    return subprocess.run(['docker', *args], capture_output=True, text=True, timeout=5, check=True).stdout


def monitor(output, workspace, duration):
    output.parent.mkdir(parents=True, exist_ok=True)
    pending = queue.Queue()
    stopping = threading.Event()
    for sig in (signal.SIGTERM, signal.SIGINT):
        signal.signal(sig, lambda *_: stopping.set())
    events = subprocess.Popen(['docker', 'events', '--format', '{{json .}}', '--filter', 'type=container',
                               '--filter', 'event=oom', '--filter', 'event=die', '--filter', 'event=destroy'],
                              stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
    def read_events():
        for line in events.stdout:
            try:
                pending.put(json.loads(line))
            except ValueError:
                pass
    reader = threading.Thread(target=read_events, daemon=True)
    reader.start()
    known = set()
    deadline = time.monotonic() + duration
    with output.open('a', buffering=1) as stream:
        def emit(kind, **values):
            stream.write(f'{json.dumps(dict(time=time.time(), kind=kind, **values))}\n')
        emit('start')
        try:
            while not stopping.is_set() and time.monotonic() < deadline:
                try:
                    mem = counters(Path('/proc/meminfo').read_text().replace(' kB', ''))
                    emit('host', kilobytes={k: mem[k] for k in ('MemTotal:', 'MemAvailable:', 'SwapTotal:', 'SwapFree:')},
                         pressure=Path('/proc/pressure/memory').read_text().strip())
                    for container in docker('ps', '-q', '--no-trunc').split():
                        fmt = '{"pid":{{.State.Pid}},"limit":{{.HostConfig.Memory}},"oom":{{.State.OOMKilled}},"mounts":{{json .Mounts}}}'
                        state = json.loads(docker('inspect', '--format', fmt, container))
                        if not any(Path(m['Source']).resolve() == workspace for m in state['mounts']):
                            continue
                        known.add(container)
                        group = cgroup_directory(Path(f'/proc/{state["pid"]}/cgroup').read_text())
                        emit('container', container=container, limit=state['limit'], oom_killed=state['oom'],
                             cgroup=str(group) if group else None, memory=read_memory(group) if group else {})
                except (OSError, ValueError, subprocess.SubprocessError) as error:
                    emit('diagnostic_error', error=type(error).__name__)
                while not pending.empty():
                    event = pending.get_nowait()
                    container = event.get('Actor', {}).get('ID', event.get('id'))
                    if container in known:
                        emit('docker_event', container=container, action=event.get('Action', event.get('status')),
                             exit_code=event.get('Actor', {}).get('Attributes', {}).get('exitCode'))
                stopping.wait(2)
        finally:
            events.terminate()
            try:
                events.wait(timeout=3)
            except subprocess.TimeoutExpired:
                events.kill()
                events.wait()
            reader.join(timeout=2)
            while not pending.empty():
                event = pending.get_nowait()
                container = event.get('Actor', {}).get('ID', event.get('id'))
                if container in known:
                    emit('docker_event', container=container, action=event.get('Action', event.get('status')),
                         exit_code=event.get('Actor', {}).get('Attributes', {}).get('exitCode'))
            emit('stop')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--workspace', type=Path, default=Path.cwd())
    parser.add_argument('--max-seconds', type=int, default=2700)
    parser.add_argument('--stop', type=Path)
    args = parser.parse_args()
    if args.stop:
        if args.stop.exists():
            pid = int(args.stop.read_text())
            command = Path(f'/proc/{pid}/cmdline')
            if command.exists():
                words = command.read_bytes().split(b'\0')
                if not any(Path(w.decode()).name == Path(__file__).name for w in words if w) or str(args.output).encode() not in words:
                    raise RuntimeError('Refusing to stop a process that does not own this monitor output')
                try:
                    os.kill(pid, signal.SIGTERM)
                except ProcessLookupError:
                    return
                deadline = time.monotonic() + 8
                while command.exists() and time.monotonic() < deadline:
                    # Orphaned monitors may remain briefly as reaped zombies.
                    stat = Path(f'/proc/{pid}/stat')
                    if not stat.exists() or stat.read_text().split(') ', 1)[1].startswith('Z '):
                        return
                    time.sleep(0.1)
                if command.exists():
                    raise RuntimeError('Memory monitor did not stop and flush within eight seconds')
        return
    monitor(args.output, args.workspace.resolve(), args.max_seconds)


if __name__ == '__main__':
    main()
