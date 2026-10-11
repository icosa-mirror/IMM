"""Resolve captured Unity SIGKILL stacks inside the matching Editor Docker image."""
import json
from pathlib import Path
import re
import subprocess
import sys


def resolve(source):
    records = [json.loads(line) for line in source.read_text().splitlines()]
    identities = {r['pid']: r for r in records if r['kind'] == 'unity_process'}
    maps = {pid: r['executable_maps'] for pid, r in identities.items()}
    maps.update({r['pid']: r['executable_maps'] for r in records
                 if r['kind'] == 'unity_final_maps' and r['executable_maps']})
    traces = [r['trace'] for r in records if r['kind'] == 'kernel_event']
    calls = {}
    results = []
    for index, line in enumerate(traces):
        context = re.search(r'-(\d+)\s+\(\s*(\d+)\)', line)
        if not context:
            continue
        tid, tgid = map(int, context.groups())
        if re.search(r'sys_(?:kill|tkill|tgkill)\(', line):
            calls[tid] = line
        if 'Unity' not in line or 'signal_generate: sig=9' not in line or 'code=0' not in line:
            continue
        target = int(re.search(r'\bpid=(\d+)', line)[1])
        result = dict(signal=line, sender_tid=tid, sender_tgid=tgid, target_pid=target,
                      same_process=target == tgid, syscall=calls.get(tid),
                      target_role=identities.get(target, {}).get('role'), frames=[])
        for frame in traces[index + 1:index + 35]:
            if '<user stack trace>' in frame:
                continue
            match = re.search(r'^\s*=>\s*<([0-9a-f]+)>', frame)
            if not match:
                break
            address = int(match[1], 16)
            resolved = dict(address=hex(address))
            for row in maps.get(tgid, []):
                fields = row.split(maxsplit=5)
                if len(fields) < 6:
                    continue
                start, end = (int(n, 16) for n in fields[0].split('-'))
                binary = Path(fields[5])
                if not start <= address < end or not binary.is_file():
                    continue
                with binary.open('rb') as stream:
                    header = stream.read(20)
                byteorder = 'little' if header[5] == 1 else 'big'
                offset = address if int.from_bytes(header[16:18], byteorder) == 2 else address - start + int(fields[2], 16)
                symbols = binary.with_name(f'{binary.name}_s.debug')
                if not symbols.exists():
                    symbols = binary
                try:
                    lookup = subprocess.run(['addr2line', '-C', '-f', '-p', '-e', str(symbols), hex(offset)],
                                            capture_output=True, text=True, timeout=30, check=True)
                    resolved.update(module=str(binary), offset=hex(offset), symbol=lookup.stdout.strip()[:1500])
                except (OSError, subprocess.SubprocessError) as error:
                    resolved['error'] = str(error)
                break
            result['frames'].append(resolved)
        results.append(result)
    return results


if __name__ == '__main__':
    print(json.dumps(resolve(Path(sys.argv[1])), indent=2))
