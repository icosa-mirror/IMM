# Optional runtime viewpoint policy and evaluated-state API

Implemented in source on 2026-10-14. Windows Release native targets and the managed runtime
compile; the CPU-only viewpoint policy smoke test passes. Other platform builds and live
Open Brush integration have not been run for this change. Updated native binaries are required
before calling the new managed methods; the checked-in package binaries are unchanged.

## Compatibility and ownership

The original player collects all spawn-area layers and allows selection independently of
visibility. It honors authored `MakeDefault` keys through a preferred-viewpoint/update signal;
the standalone viewer then selects that preference and aligns its camera. Those semantics,
legacy enumeration/setters, and the standalone viewer are unchanged.

The new policy is optional. Availability means the spawn-area layer and every ancestor are
visible in the current evaluated layer state, including playback visibility overrides. It does
not create chapter ownership, use opacity as visibility, or restrict the legacy setters.
Pausing does not make a viewpoint unavailable. Document Hide is a separate render/playback
control; availability queries describe the last evaluated layer hierarchy, not whether the
host is currently displaying the document.

IMM evaluates timelines, inherited visibility, and poses. The host chooses whether to opt in,
refreshes controls, aligns the scene, and decides what to do when an active/preferred viewpoint
is unavailable. IMM never automatically replaces an unavailable active selection.

## Managed API

These methods are on `ImmDocument`:

| Method | Contract |
|---|---|
| `IsSpawnAreaAvailable(id)` | False for an invalid/unloaded/unevaluated document or ID; otherwise evaluated world visibility |
| `GetAvailableSpawnAreaIds()` | Allocating convenience query; returns eligible spawn IDs in existing document order |
| `GetAvailableSpawnAreaIds(int[] buffer)` | Allocation-free caller-buffer query; returns total required count, or -1 when unavailable; writes up to buffer length; null requests count only |
| `TryGetSpawnAreaSnapshot(int[] buffer, out SpawnAreaSnapshot state)` | Atomically reads available IDs, active/preferred IDs and poses, update flag, chapter, and evaluation/request state; null requests header/poses only |
| `TrySetActiveSpawnAreaId(id)` | Optional checked selection; preserves active selection on failure |
| `TrySetActiveSpawnAreaId(id, out SpawnAreaPose pose, ulong expectedEvaluationRevision = 0)` | Same check, returning selected IMM-space pose under the same lock; a nonzero stale evaluation revision rejects the selection |
| `RequestSetChapter(chapterIndex)` | Queues an opt-in tracked chapter command; returns a nonzero document-local token, or zero if invalid, hidden, unloaded, or command slot busy |
| `RequestSetTime(startTicks, stopTicks)` | Queues a tracked seek using existing SetTime tick units/argument meanings; nonnegative values required; returns token or zero |

IDs are the same spawn-area indices used by the legacy APIs, not layer IDs. Clients do not need
to map IDs to layers to use availability. IDs/tokens are scoped to the loaded document and must
be discarded on unload/replacement. Never infer persistent identity across reload.

The snapshot buffer may be reused every frame. A short buffer yields false but still supplies
valid header/poses and `availableCount`; its prefix is filled in document order. Grow the buffer
and retry if a complete list is needed. Invalid/unloaded/unevaluated state yields false and
must not be used. Count and list calls made separately are not an atomic snapshot; prefer the
snapshot API when coordinating controls and movement.

`evaluationRevision` increments when the CPU timeline is evaluated, not on every query or
selection. It is not an availability-change notification, save revision, or general state
version. Polling the snapshot can avoid allocations, but a changing evaluation revision does
not necessarily mean the eligible list changed. Visibility checks walk ancestor chains without
reevaluating timelines. No new per-frame cache or second timeline evaluator is introduced.
Checked selection always validates current visibility under the player lock and rejects while
an opt-in tracked command is pending. A supplied revision additionally catches intervening CPU
evaluation. The returned pose avoids a selection/pose-read race; it cannot freeze future
playback while the host moves its scene.

## Completion and command ordering

The original `SetChapter` remains queued; original `SetTime` remains immediate. They have no new
completion promise. Use the new request methods when a host needs explicit completion.
They use the existing single document command slot and reject busy slots rather than replacing
another command. Only one tracked request may be pending. Requests require continued player
updates; waiting synchronously on the render/main thread would prevent completion.

1. Store the nonzero token returned by a request. Zero means nothing was queued.
2. Poll snapshots. `requestedCommand` identifies the most recent tracked request;
   `completedCommand` identifies the most recently resolved tracked request.
3. `state.IsComplete(token)` is true only when that exact token is `Applied`, after evaluation
   of visibility, authored viewpoint actions, and transforms.
4. A matching `Rejected` or `Superseded` status is terminal. A different `requestedCommand`
   means the caller's token is no longer the latest request. This API is not a request history.
5. Legacy queued controls can overwrite the single slot. Such a tracked request becomes
   `Superseded` when the document next processes commands. Unload invalidates the snapshot/token.

Statuses are `None=0`, `Pending=1`, `Applied=2`, `Rejected=3`, and `Superseded=4`.
Tracked chapter selection retains existing chapter-navigation playback semantics, including
unpausing. Tracked seek evaluates even while paused and reapplies the normal pause policy to
newly activated layers before returning; it does not resume playback. Pending/rejected status
is distinct from having a valid earlier evaluated snapshot.

## Host integration sequence

1. During normal playback, read a snapshot into a reusable buffer. Refresh cycling controls
   from its available IDs; ordinary visibility changes do not raise the MakeDefault flag.
2. On a user selection, call checked selection and align only on success, using its returned
   pose. Passing the snapshot evaluation revision provides an additional stale-state check;
   if it fails, refresh and retry through normal host control flow.
3. For chapter/seek navigation, use a tracked request and wait for that token's evaluated
   completion before refreshing and aligning. Do not treat command acceptance as completion.
4. On `needsUpdate`, inspect the preferred ID and availability. The host may select/align an
   eligible preference, or explicitly defer/ignore an unavailable preference. Checked selection
   does not clear the authored request. Clear it with the existing update-flag setter only
   after successful alignment or an explicit host decision to ignore it.
5. Preserve saved-sketch placement on restoration. Decide explicitly whether to acknowledge or
   defer a pending authored request so it cannot inadvertently overwrite the restored placement.
6. Follow the active evaluated pose during playback if the host supports animated viewpoints.
   If the active viewpoint becomes unavailable, retaining placement until an eligible authored
   change or successful user selection is a host policy, not an IMM automatic transition.

The existing Boolean update flag is not a generation-tagged acknowledgment API: a later
MakeDefault can arrive between host alignment and clearing the flag. Hosts should coordinate
reads/selection/alignment/acknowledgment in their normal player-update phase. This change does
not alter that legacy signal or claim acknowledgment is atomic with scene movement.

## Native ABI

Definitions are in `code/libImmPlayer/src/spawnAreaState.h`; the Unity plugin exports the six new
entry points with its existing platform calling convention. Policy booleans use explicit
32-bit results. `SpawnAreaPoseState` is 44 bytes, matching managed `SpawnAreaPose`.
`SpawnAreaSnapshot` is 160 bytes, version 1, and starts with size/version. Both active/preferred
poses are zero when their ID is negative. Poses remain valid even if their viewpoint is hidden;
the separate availability fields determine whether the optional policy permits selection.

Native signatures:

```cpp
int GetAvailableSpawnAreaIds(int docId, int capacity, int* ids);
int IsSpawnAreaAvailable(int docId, int spawnAreaId);
int TrySetActiveSpawnAreaId(int docId, int spawnAreaId,
    uint64_t expectedEvaluationRevision, SpawnAreaPoseState* pose);
int GetSpawnAreaSnapshot(int docId, SpawnAreaSnapshot* state, int capacity, int* ids);
uint64_t RequestSetChapter(int docId, int chapterIndex);
uint64_t RequestSetTime(int docId, int64_t startTicks, int64_t stopTicks);
```

List/snapshot calls return total required count or -1; selection/availability return 1/0.
Snapshot input requires `structSize >= 160` and `structVersion == 1`. Negative capacities,
nonzero capacities with null buffers, and invalid snapshot descriptors are rejected. New
player methods validate document slots and serialize reads/checks/requests against player
updates. As with the existing player API, native initialization must precede document calls.

## Validation

Build `libImmPlayer` and `appImmUnity` Release first, disabling automatic Unity package copying
when another Editor/agent is using the package. Then run:

```powershell
python code/appImmUnity/tests/viewpoint_policy_smoke.py --msbuild "<path to MSBuild.exe>"
```

The smoke runner exports a synthetic group/spawn-only IMM fixture, builds an isolated CPU-only
executable using the native target's link settings, and loads/evaluates it without a renderer
or Unity Editor. It checks inherited/self/root visibility, invalid IDs/buffers, empty/short
lists, snapshot consistency, checked selection/stale revisions, unrestricted legacy selection,
paused seek completion, evaluated parent pose, MakeDefault preference versus active selection,
chapter completion/rejection, command supersession/temporary masking, and unload invalidation. Build/runtime logs
are written under `artifacts`; no package binary is replaced.
