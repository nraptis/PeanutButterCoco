# Relay Point + Batch Execution Spec (Draft v3)

Status: draft  
Date: 2026-03-30  
Scope: `ArchiverEngine` + command-bus integration

## Why this exists

This document now tracks both batch execution and the engine-shape correction.
Batching remains important, but `Heartbeat()` and `Poll()` remain first-class APIs and are not being removed.

Target model:

1. Engine instances are action-specific (`Bundle`, `Decode`, `Repair`, `Sanity`).
2. `ArchiverExecutor` is the UI-facing orchestrator that receives requests, loops heartbeat, and handles inbound commands (including cancel/new requests while active).
3. `Heartbeat()` advances deterministic work units/batches.
4. `Poll()` drains emitted events/log/progress/runtime notifications.
5. Relay/checkpoint responses remain supported but should be rare and short-circuit oriented.

## Hard requirements

1. `Heartbeat()` and `Poll()` are normative API surface and must remain available.
2. Bundle and decode progression is step/batch based.
3. Execution is single-threaded and linear (no per-block worker thread fan-out).
4. A batch has a bounded event stack lifetime: created at batch start, cleared after batch completion.
5. Responses are correlated by `actionRunId + relayId` and stale/orphan responses are ignored deterministically.
6. UI gets a blocking receive/analyze/respond cycle at defined check-in points.
7. Bundle / Unbundle / Repair must share an explicit cancel boundary contract.
8. The UI may replace one active engine with another only after dispose/cleanup acknowledgment is observed.
9. Engine action lock-in inside one monolithic runtime is deprecated in favor of action-specific engine classes.
10. UI talks to `ArchiverExecutor` via `ArchiverExecutorDelegate`; engine-level tests may bypass executor and drive engines/tasks directly.

## Terms

1. `Action Run`: one accepted primary action (`Bundle`, `Decode`, `Manifest`, `Repair`, `Sanity`).
2. `Batch`: the count-bounded set of block actions executed before checking back in with UI/commands.
3. `Relay Point`: named hook definition that may emit a relay request.
4. `Relay Request`: payload emitted at a relay point.
5. `Relay Response`: decision payload returned by relay host.
6. `Batch Event Stack`: transient vector of runtime and hook events scoped to one batch.
7. `Gap Archive`: an empty archive box/sector that has no decodable payload for recover output.
8. `Work Unit`: the minimum boundary where cancel/log/progress/error/event checks are performed.

## Inconsistencies observed (current code vs previous spec)

1. Previous draft claimed heartbeat should become a compatibility shim only, but implementation still uses heartbeat burst slicing and poll-driven event retrieval as core progression.
2. Previous draft centered a single `ArchiverEngine` that switches primary action/task internally; this creates action lock-in and broad internal branching.
3. Previous draft emphasized detachable relay flow for many decisions; in practice most decisions should short-circuit locally and relays/checkpoints should be uncommon.
4. Previous draft migration items asked to remove fixed-rate heartbeat scheduling wording; this conflicts with keeping `Heartbeat()/Poll()` as stable external control surface.

## Superseding engine shape (normative)

The architecture direction is:

```cpp
class ArchiverEngineBase;
class ArchiverEngine_Bundle;
class ArchiverEngine_Decode;
class ArchiverEngine_Repair;
class ArchiverEngine_Sanity;
```

Required base contract:

1. `Heartbeat()` advances at most bounded work for that action engine.
2. `Poll()` returns queued events/logs/progress/runtime events since last poll.
3. `Dispose()` requests teardown and returns quickly.
4. Cleanup acknowledgment must be observable (`IsDisposed()` or terminal disposed event).
5. `EngineType()` is always available for explicit UI-side type checks.

Recommended engine type enum:

```cpp
enum class EngineTypeV2 {
  kUnknown = 0,
  kBundle = 1,
  kDecode = 2,
  kRepair = 3,
  kSanity = 4,
};
```

UI replacement lifecycle:

1. If an active engine exists, call `Dispose()`.
2. Move it to a kill queue until cleanup acknowledgment is seen.
3. Null active pointer.
4. Create the next action-specific engine instance.
5. Drive it with `Heartbeat()` and consume output with `Poll()`.

## Executor contract (normative)

`ArchiverExecutor` sits between UI and engines.

Responsibilities:

1. Own command bus + active engine.
2. Receive UI requests (`bundle`, `decode`, `manifest`, `repair`, `sanity`, `cancel`, prompt responses, checkpoint decisions).
3. Advance execution loop with heartbeat ticks.
4. Notify UI through `ArchiverExecutorDelegate` when items are available.
5. Keep overlap handling deterministic by relying on command ordering and explicit accept/reject engine events.

Testing rule:

1. UI/integration tests may use `ArchiverExecutor`.
2. Engine/task tests should run engines/tasks directly and should not require executor wiring.

## Batch size knobs (normative)

```cpp
constexpr std::uint32_t kBatchSizeBundle = 10u;
constexpr std::uint32_t kBatchSizeDecode = 10u;
constexpr std::uint32_t kBatchSizeBundleRepair = 10u;
constexpr std::uint32_t kBatchSizeRepair = 10u;
```

Bundle rule:

`batch_size = min(kBatchSizeBundle, remaining blocks in current file)`

Decode rule:

`batch_size = min(kBatchSizeDecode, remaining decodable blocks in current source unit)`

Bundle repair packing rule:

`batch_size = min(kBatchSizeBundleRepair, remaining repair blocks before EOF)`

Repair-action decode rule:

`batch_size = min(kBatchSizeRepair, remaining repair-walk blocks before EOF)`

Operational definition:

`batch_size` is "how many block actions we will do before checking back in with the UI/commands."

## UI check-in cycle (blocking)

UI enters receive/analyze/respond at these boundaries:

1. preflight finishes
2. discovery finishes
3. any batch finishes
4. any phase finishes (for example preview-manifest phase, manifest phase)

At each check-in:

1. engine flushes batch/phase events
2. engine drains inbound commands and relay responses
3. engine applies accepted decisions
4. engine either starts next batch/phase or transitions to terminal state

## Bundle step contract (normative)

Given current file cursor state:

1. Compute `batch_size` with the bundle rule.
2. Capture cursor snapshot at step start:
   - source file identity
   - byte/block offset in file
   - archive/section destination coordinates
3. Fill payload buffers by chugging file data until `batch_size` blocks are staged.
4. Record all batch events into the batch event stack.
5. Process each staged block sequentially (encrypt + write/save) in the same execution thread.
6. Check back in with UI/commands (drain inbound command queue + relay responses).
7. Dispatch UI relay notification for the batch when needed and wait for required responses for this batch.
8. Resolve batch result (`finished`, `running`, `failed`, `canceled`).
9. Flush the batch event stack.
10. Clear event stack memory for next batch.

## Required bundle batch events

The batch event stack MUST support at least:

1. file started going into bundle
2. file finished going into bundle
3. archive block written
4. repair block written
5. section header written
6. section header skip record written
7. file started writing into preview manifest
8. file finished writing into preview manifest
9. folder started writing into preview manifest
10. folder finished writing into preview manifest
11. folder started writing into manifest
12. folder finished writing into manifest
13. batch finished flag / outcome marker

## Decode step contract (normative)

Given current decode cursor state:

1. Compute `batch_size` with the decode rule.
2. Capture decode cursor snapshot at step start:
   - source archive/block coordinates
   - destination file/folder cursor coordinates when applicable
3. Process up to `batch_size` decode blocks sequentially in one thread.
4. Record all decode events (including recover/error events) into the batch event stack.
5. Check back in with UI/commands (drain inbound command queue + relay responses).
6. Dispatch UI relay notification for the batch when needed and wait for required responses for this batch.
7. If any decode error event occurs, terminate the current batch immediately.
8. Resolve batch result (`finished`, `running`, `failed`, `canceled`).
9. Flush and clear the batch event stack.

## Required decode batch events

The decode batch event stack MUST support at least:

1. block read
2. block decoded
3. block missing
4. block bad checksum
5. recover skipped to file/block/byte
6. file started writing to disk
7. file finished writing to disk
8. file encountered name error
9. file encountered data error
10. preview record started skip
11. preview record finished skip
12. empty folder encountered name error
13. empty folder finished writing to disk
14. file closed as partial
15. file discarded due to name-boundary error

## Error-flow separation (normative)

Decode error events terminate the current batch immediately.

Rules:

1. `block missing`, `block bad checksum`, `recover skipped`, `file encountered * error`, and `empty folder encountered name error` end the active batch.
2. In recover mode, batch termination does not necessarily terminate the action; the director computes the next recover-walk cursor and starts a new batch.
3. In non-recover decode modes, policy may escalate batch termination to action terminal failure.
4. Emit the batch error event(s) before any terminal action event.
5. Recover-salvage implementation must request a batch-yield boundary immediately after accepting a damaged block so no additional block decode occurs in the same batch.

## Cancel boundary contract (director-level)

Intent:

`finish current user file, then honor cancel, then finalize safely`

Rules:

1. Cancel can be requested at any time; engine marks cancel as pending.
2. Directors do not abort in the middle of a user file write/decode unit unless there is a hard integrity stop.
3. When current user file reaches its cancel boundary, the active batch ends.
4. After boundary:
   - if action is Bundle, run safe finalization path (header/metadata consistency path), then cancel terminal.
   - if action is Unbundle/Decode, close/flush partial outputs safely per policy, then cancel terminal.
   - if action is Repair, finalize repair outputs safely, then cancel terminal.
5. If cancel arrives during preflight/discovery/phase work with no active user-file write, honor cancel at next check-in boundary.

## What counts as a batch unit

Examples of "batch units" in this model:

1. recovery walk repeatedly encountering invalid blocks / bad checksums (up to batch size before check-in)
2. packing blocks for preview/manifest/file payloads
3. unpacking/decoding blocks into output

## Cursor completeness contract

Because an archive may repeatedly miss clean logical-record boundaries, cursors MUST be able to pause and resume mid-parse safely.

Required persisted cursor state includes:

1. active action/decode mode and phase
2. logical zone/record mode (file data, folder record, preview-manifest record, etc.)
3. current output target identity:
   - resolved full output path when known
   - whether target is file vs folder
   - open/closed state
4. current record parser position:
   - record-kind being decoded
   - bytes consumed in current record
   - bytes remaining in current record payload
5. integer subcursor:
   - integer semantic kind (what this number means)
   - integer width in bytes
   - endianness
   - partial bytes already read/written
   - partial-byte buffer
6. name/data subcursor:
   - partial name bytes buffer
   - name parse completeness flag
   - data payload write offset
7. recover-walk cursor:
   - current naive walk position
   - last verified skip target (if any)
   - jump-applied flag for current decision

## Integer subcursor requirements

Batch boundaries are allowed during numeric parsing, so the integer subcursor is mandatory.

Rules:

1. Every in-progress integer parse must carry semantic kind metadata (not just "some uint32").
2. Resume logic must continue from partial-byte count, not restart integer parse.
3. On parse failure, emit decode error event and terminate batch.
4. Recover mode then uses recover-walk policy to continue in next batch.

## Decode partial-file closure policy

Goal: gracefully close off files that hit error boundaries, except name-boundary failures.

Rules:

1. If error boundary occurs during file data write:
   - flush and close current file handle
   - mark output as partial
   - emit `file encountered data error`
   - emit `file closed as partial` with written-byte count
2. If error occurs during name parsing/before path is valid:
   - do not create/finalize output file
   - emit `file encountered name error`
   - emit `file discarded due to name-boundary error`
3. In recover mode:
   - continue recover walk after each batch-ending error
   - multiple partial files may be produced in one action run
4. In non-recover decode modes:
   - policy may stop action after first fatal error boundary
5. File handles/resources must be closed at every batch end and terminal transition.

## Cursor design and risks

Main cursor trouble areas:

1. pausing in the middle of numeric fields
2. pausing in the middle of name/data record parsing
3. pausing while switching file/folder/preview-manifest modes
4. recover-walk cursor and decode cursor drifting out of sync

Recommended simplification:

1. Cursors must support batch boundaries in the middle of fixed-width numbers and logical record headers.
2. Prefer batch boundaries at block boundaries or completed logical-record boundaries for readability, but correctness must not depend on that preference.
3. Keep recover-walk cursor independent and explicit: naive position + optional verified jump target.
4. Keep file writer cursor explicit: current output file id + byte offset + open/closed state.

Answer to your question:

Yes, that approach is simpler, but the format hardline is stricter: we must be correct even when paused mid-number or mid-header.

## Recover walk contract (recover mode)

The recover director follows a naive march-forward cursor with one jump exception.

Rules:

1. Default behavior is naive walk forward to the next candidate block/sector.
2. Jump ahead is allowed only when both are true:
   - checksum/skip record is verified as valid
   - skip target points to a valid in-range location in the archive set
3. If skip target is invalid/out-of-range, continue naive walk forward.
4. If skip target points into a gap archive, move the naive-walk cursor to the next non-gap archive after that gap archive.
5. If no next non-gap archive exists, recover walk ends and the action resolves per recover completion policy.

## Relay message contract (normative)

```cpp
enum class RelayRequestModeV1 {
  kObserve = 0,  // no response required
  kDecide = 1,   // response required
};

enum class RelayDecisionV1 {
  kContinue = 0,
  kCancel = 1,
  kClear = 2,
  kMerge = 3,
};

struct RelayRequestV1 {
  std::uint64_t mRelayId = 0u;        // unique within action run
  std::uint64_t mActionRunId = 0u;    // increments on ActionAccepted
  std::uint64_t mBatchId = 0u;        // current batch id
  std::string mRelayPointId;          // stable id
  RelayRequestModeV1 mMode = RelayRequestModeV1::kObserve;
  RuntimeEventV2 mRuntimeEvent{};     // optional source event
  UiPromptRequestV2 mPrompt{};        // optional prompt data
  std::vector<RuntimeEventInfoV2> mInfo;
  std::uint64_t mDeadlineUnixMs = 0u;
};

struct RelayResponseV1 {
  std::uint64_t mRelayId = 0u;
  std::uint64_t mActionRunId = 0u;
  std::uint64_t mBatchId = 0u;
  RelayDecisionV1 mDecision = RelayDecisionV1::kContinue;
  std::vector<RuntimeEventInfoV2> mInfo;
};
```

## Canonical relay points (v2)

| Relay point id | Trigger | Mode | Allowed responses |
|---|---|---|---|
| `ui.destination_action` | bundle/decode destination conflict | `kDecide` | `kClear`, `kMerge`, `kCancel` |
| `runtime.checkpoint` | configured blocking runtime event | `kDecide` | `kContinue`, `kCancel` |
| `batch.ready_for_ui` | batch staged and dispatched | `kObserve` | n/a |
| `batch.completed` | batch compute + response barrier completed | `kObserve` | n/a |
| `runtime.event_observer` | emitted runtime event | `kObserve` | n/a |

## Lifecycle rules

1. On action accept:
   - increment `mActionRunId`
   - reset pending relay requests
   - reset `mBatchId` to 0
2. On batch start:
   - increment `mBatchId`
   - allocate empty batch event stack
3. On relay request emit:
   - assign `mRelayId`
   - key pending map by `(actionRunId, batchId, relayId)`
4. On relay response receive:
   - resolve only when key matches pending request
5. On batch completion:
   - flush event stack
   - clear stack contents
6. On action terminal:
   - clear all pending relay requests
   - reject later responses as orphaned

## Stale/orphan handling

Ignored responses MUST emit one reason:

1. `unknown_relay_id`
2. `action_run_mismatch`
3. `batch_mismatch`
4. `already_resolved`
5. `deadline_expired`
6. `action_terminal`

## Execution model (single-threaded)

1. All bundle/decode block work in a batch executes sequentially on one engine execution thread.
2. UI relay notifications/responses are message-based coordination points, not worker-thread fan-out.
3. Batch completion barrier requires:
   - sequential block loop finished for the batch
   - all required relay decisions resolved
4. Batch event stack is single-owner and never shared across batches.

## Migration plan

1. Introduce batch ids and relay ids in messaging.
2. Split monolithic action switching into `ArchiverEngineBase` + action-specific engines.
3. Implement bundle batching with `kBatchSizeBundle`.
4. Implement decode batching with `kBatchSizeDecode`.
5. Keep prompt/checkpoint relay points, but default to local short-circuit for common paths.
6. Keep `Heartbeat()` and `Poll()` as stable execution APIs (not temporary shims).
7. Add UI disposal/kill-queue handoff and cleanup acknowledgment handling.

## Acceptance criteria

1. Bundle step computes `batch_size = min(kBatchSizeBundle, remaining file blocks)`.
2. File cursor resume position is preserved exactly across batches.
3. All required bundle batch event types are emitted and scoped to one batch.
4. Batch event stack is cleared after each batch and does not leak into next batch.
5. No per-block worker threads are required; block work stays linear and deterministic.
6. Any decode error event terminates the active decode batch immediately.
7. In recover mode, post-error continuation follows recover-walk rules (naive forward unless valid verified skip jump applies).
8. Gap-archive skip targets advance to the next non-gap archive; invalid skip targets fall back to naive walk.
9. Cursor state can resume from mid-integer and mid-record boundaries without re-reading from record start.
10. Name-boundary errors do not emit finalized files; data-boundary errors close files as partial.
11. Recover mode may emit multiple partial files in one action while continuing recover walk.
12. Stale/orphan relay responses are ignored with reason codes.
13. Engine progression is externally driveable via `Heartbeat()` and externally observable via `Poll()`.
14. UI can replace active engine instances safely using dispose + kill queue + cleanup acknowledgment.
15. Engine type is explicit at runtime (`Bundle`, `Decode`, `Repair`, `Sanity`) without relying on hidden task state.
