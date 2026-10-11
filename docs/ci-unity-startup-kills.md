# Intermittent Unity Linux startup kills

Root cause remains unresolved. A single permitted retry is recovery, not a fix.

## Evidence

1. Unity exits 137 during its first assembly reload, after registering precompiled
   DLLs and before project compilation. The shell reports `Killed`.
2. Affected jobs build Windows and Android players on `ubuntu-latest`.
3. Run 38150209754, Android job 114504756074, failed its build step after 3m30s,
   below the configured 30-minute step and 120-minute job timeouts.
4. That container's recorded memory peak was 2,874,634,240 bytes against a
   15,927,869,440-byte limit. Its cgroup reported zero `oom`, `oom_kill`, and
   `max` events; Docker reported no OOM kill. Earlier Windows failures similarly
   stayed well below their limits. These records do not identify the kill sender.
5. In a sample of 16 recent validation runs, the affected Windows player-build
   and Android jobs had 8 exit-137 failures and 22 successes on their first
   completed attempts; 9 cancelled jobs were excluded. Two failed jobs completed
   one retry and both passed. The sample is small and cancellation-biased.

## Signal evidence on subsequent builds

The existing memory monitor starts `trace_unity_kill.py` with passwordless sudo.
It uses a private kernel trace instance and enables only SIGKILL generation and,
when available, OOM victim selection. It does not change resource limits, Unity
arguments, renderer behavior, or retry policy.

1. Inspect `memory-*-signals.jsonl` in the affected job's existing evidence
   artifact. `ready` confirms setup; `unavailable` records setup failure.
2. Each tracer deliberately kills one owned `sleep` child. Its `self_test` record
   identifies the sender and target PIDs. `self_test_result.observed` confirms the
   kernel trace saw it. This event is diagnostic and must not be mistaken for a
   Unity failure. The Linux CI infrastructure test also verifies this probe.
3. For other `signal_generate` records, the trace prefix identifies the generating
   task and PID; the payload identifies the target task/PID and signal code.
   Correlate a killed Unity task with the Editor log and memory timestamps.
   An OOM victim event supplies separate kernel OOM evidence. A signal generated
   in kernel context requires further interpretation; the prefix alone must not
   be assumed to identify a userspace `kill()` caller.
4. The tracer records no command-line arguments, environment variables, license
   contents, or general syscall streams. Shutdown disables its events and removes
   only its own trace instance.

If Unity fails without a matching signal record despite a successful self-test,
investigate trace loss and the editor/wrapper exit path before attributing the
failure to GitHub infrastructure. Do not change resource limits or add repeated
retries without evidence linking them to the failure.
