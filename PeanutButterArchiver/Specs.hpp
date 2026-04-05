#pragma once

/*
PBTR Unified Spec (Code + Draft Consolidation)
===============================================

Status
- This file is the single in-project spec document requested for Xcode reading.
- It consolidates current code behavior plus active draft docs.
- Date: 2026-03-29

------------------------------------------------------------------------------
0) PRECEDENCE / SOURCE OF TRUTH
------------------------------------------------------------------------------

Use this precedence when sources disagree:

P0 (highest): Implemented code in `PeanutButterArchiver/Code/*`
- Format structs, constants, parser behavior, cancel/finalize behavior, UI wiring.

P1: `relay_points_spec.md`
- Target design for batch/relay/check-in contract.
- Authoritative for intended relay-point ownership, stale response handling, and
  explicit batch model during migration.

P2: `instability.txt`
- Open-product-contract risk register.
- Use to track what is intentionally not finalized.

P3 (lowest): `spec.txt`
- Broad format intent and architecture notes.
- Useful for policy direction; not fully implemented.

If this file and code diverge, code wins until code is changed.

------------------------------------------------------------------------------
1) CONSTANTS / KNOBS (CURRENT CODE)
------------------------------------------------------------------------------

Defined in `Code/Knobs.hpp` and memory-layout headers:

- `kDefaultBlocksPerArchiveV2 = 5`
- `kBatchBudgetBytesBundleV2 = kSectionPayloadBytesV2`
- `kBatchBudgetBytesDecodeV2 = kSectionPayloadBytesV2`
- `kBatchBudgetBytesBundleRepairV2 = kSectionPayloadBytesV2`
- `kBatchBudgetBytesRepairV2 = kSectionPayloadBytesV2`
- `kBatchSize*V2` are derived from batch budget bytes via
  `ceil(budget_bytes / kSectionPayloadBytesV2)`

- `kArchiveHeaderBytesV2 = 64`
- `kSectionHeaderBytesV2 = 96`
- `kArchiveBlockBytesV2 = 1,044,480`
- `kSectionPayloadBytesV2 = 1,044,384`
- `kMaxPathLengthV2 = 16,384`

- Default archive-layout config:
  - max archive count: `1,048,576`
  - max blocks per archive: `2,048`

- Archive suffix: `.PBTR`

------------------------------------------------------------------------------
2) PRIMARY ACTIONS / ENGINE PROTOCOL
------------------------------------------------------------------------------

Primary actions (`EnginePrimaryActionV2`):
- `kNone`, `kBundle`, `kDecode`, `kManifest`, `kRepair`, `kSanity`

Command types (`EngineCommandTypeV2`):
- `kStartBundle`
- `kStartDecode`
- `kStartManifest`
- `kStartRepair`
- `kStartSanity`
- `kCancel`
- `kPromptResponse`
- `kCheckpointDecision`

Event types (`EngineEventTypeV2`):
- `kActionAccepted`, `kActionRejected`
- `kCancelAccepted`, `kCancelRejected`
- `kUiStateChanged`
- `kLog`
- `kProgress`
- `kActionCompleted`, `kActionFailed`, `kActionCanceled`
- `kRuntimeEvent`
- `kCheckpointRequested`

Important current behavior:
- Primary-action support is filtered by engine type (`base`, `bundle`, `decode`,
  `repair`, `sanity`) via `SupportsPrimaryActionLocked(...)`.
- In the base engine, bundle/decode/manifest/repair/sanity actions are all
  accepted.

------------------------------------------------------------------------------
3) ON-DISK FILE FORMAT (CURRENT CODE)
------------------------------------------------------------------------------

3.1 Archive Header (`ArchiveHeaderV2`, 64 bytes)
Offset map (little-endian numeric fields):

- `0..7`: `mMagic` (`0x5045414E55544254`, "PEANUTBT")
- `8`: `mArchiveFormatVersion` (non-zero required; code does not enforce exact `2`)
- `9`: `mCipherVersion`
- `10`: `mExpanderVersion`
- `11`: `mDirtyState`
- `12`: `mIsEncrypted` (`0` or `1`)
- `13`: `mCipherProfile`
- `14`: `mExpanderProfile`
- `15`: `mReserved0`
- `16..21`: `mArchiveIndex` (packed uint48)
- `22..27`: `mArchiveCount` (packed uint48)
- `28..33`: `mBlockCountMain` (packed uint48)
- `34..39`: `mReservedCount0` (packed uint48, currently written as `0`)
- `40..45`: `mBlockCountPreview` (packed uint48)
- `46..51`: `mBlockCountRepair` (packed uint48)
- `52..59`: `mArchiveFamilyId` (u64)
- `60..63`: `mReserved1` (u32)

Dirty states (`ArchiveDirtyStateV2`):
- `0`: invalid/provisional
- `1`: finished with cancel
- `2`: finished with error
- `3`: finished with cancel and error
- `4`: finished

Bundle writes provisional headers first (`kInvalid`), then patches to final state.

3.2 Section Header (`SectionHeaderV2`, 96 bytes)
Offset map:

- `0..31`: `mCheckSum` (32 bytes)
- `32..39`: `mSkipRecord` (8 bytes)
- `40..43`: `mRepairRecord` (4 bytes)
- `44`: `mCheckSumKind` (must match SHA-256 kind constant)
- `45`: `mSectionType`
- `46`: `mSectionFlags` (must be zero)
- `47..50`: `mPayloadBytesUsed` (u32)
- `51..54`: `mArchiveFileCount` (u32)
- `55..58`: `mArchiveBlockCount` (u32)
- `59..62`: `mArchiveIndex` (u32)
- `63..66`: `mBlockIndex` (u32)
- `67..72`: `mBlockCountMain` (packed uint48)
- `73..78`: `mBlockCountPreview` (packed uint48)
- `79..84`: `mBlockCountRepair` (packed uint48)
- `85..92`: `mArchiveFamilyId` (u64)
- `93..95`: reserved bytes

Section types (`SectionTypeV2`):
- `0`: archive_data
- `1`: preview_manifest
- `2`: reserved (unused)
- `3`: repair_data

3.3 Skip Record (`SkipRecordV2`, 8 bytes)
- `mArchiveIndex` (packed u24, absolute archive index)
- `mBlockIndex` (u16, local block index inside that archive)
- `mByteIndex` (packed u24, payload byte offset inside target block)

3.4 Repair Record (`RepairRecordV2`, 4 bytes)
- `mArchiveIndex` (u16)
- `mBlockIndex` (u16)
- Non-repair (preview/data) blocks carry intentionally invalid repair targets.
- Repair-copy blocks carry valid archive-local targets for the block they repair.

3.5 Section checksum
`ComputeSectionCheckSum` hashes:
- full payload span (section payload bytes), plus
- section metadata (skip, repair, section type, flags, payload-used, counts, IDs)
using an FNV-style state/mix pipeline into 32 bytes.

3.6 Typed logical records (stream grammar)
Record type flags (`TypedRecordTypeV2`):
- `1`: manifest file
- `2`: manifest folder
- `3`: data file
- `4`: data folder

Record shape:
- `path_length_le16`
- `path_bytes[path_length]`
- `type_flag_u8`
- if file-type: `file_size_le64`
- if type has content bytes (currently only `data_file`): file content bytes

Decoder path safety checks (current):
- non-empty
- <= max path length
- no absolute/rooted path
- no drive-letter root
- no empty path components
- forbids exact `"."` and `".."` path segments
- rejects control bytes (`<32`), DEL, NUL

------------------------------------------------------------------------------
4) MIXED-TYPE / ZONE RULES
------------------------------------------------------------------------------

Current implementation:
- Each block has exactly one section type (`SectionHeaderV2::mSectionType`).
- Across blocks, section types are mixed by plan/order.

Bundle non-repair order is:
- preview-manifest blocks next (if enabled)
- archive-data blocks next
- repair blocks appended later in repair phase

Logical-record mix:
- Preview-manifest zone: manifest-folder + manifest-file records.
- Data zone: data-file + data-folder records in current bundle path.
- Empty folders are carried in data zone as `data_folder` records (not as dedicated
  empty-folder section blocks).

Preview block encryption:
- Preview-manifest blocks are written plaintext even when archive encryption is on.
- Decode has explicit fallback to try plaintext preview header parse before unseal.

------------------------------------------------------------------------------
5) BUNDLE DATA FLOW (CURRENT)
------------------------------------------------------------------------------

Bundle phase order:
1. Preflight
2. Discovery
3. MemoryPlanning
4. DeriveCipherMaterial (if encryption enabled)
5. AssembleCipherStack (if encryption enabled)
6. ArchiveManifest
7. ArchivePacking
8. RepairPacking (if repair enabled)
9. FinalizingHeaders

Key flow details:
- Discovery builds file records + empty-folder records.
- Discovery dereferences source links while packing content (archives include materialized bytes, not link metadata).
- Directory-link recursion is cycle-guarded to avoid infinite walk when links point back to an ancestor.
- Memory planning computes block counts and archive file layout.
- Archive packing emits runtime events for archive/block/header/encryption/file/folder.
- Repair packing copies selected front-layer blocks into repair area.
- Finalizing headers patches dirty state and counts.

Current batch behavior:
- Archive packing loop uses `kBatchSizeBundleV2` block budget per heartbeat slice.
- Repair packing loop uses `kBatchSizeBundleRepairV2`.

Cancel behavior (current code):
- Cancel is observed at block boundaries.
- Not guaranteed to always finish current logical user file before honoring cancel.
- Bundle may finalize headers as canceled and stop with partial archive content.

------------------------------------------------------------------------------
6) DECODE / UNBUNDLE / RECOVER FLOW (CURRENT)
------------------------------------------------------------------------------

Decode phase order:
1. Preflight
2. HeaderBootstrap
3. Discovery
4. DeriveCipherMaterial
5. AssembleCipherStack
6. Inspection
7. ManifestDiscovery
8. ArchiveDecode
9. Finalize

Read Manifest mode:
- Uses decode preflight/bootstrap/discovery/manifest-discovery path.
- Current report is header/discovery-centric and warns preview counts are advertised.
- It does not decode preview payloads for report truth.

Recover behavior (decode intent recover):
- Recover is Unbundle-with-salvage only.
- Decode archive phase continues past damaged blocks (salvage path).
- On damaged block: switches to pessimistic mode, emits skip-jump marker, requests
  batch-yield boundary (`RequestBatchYield()`), continues on next slice.
- Recover does not consume repair blocks as replacement payload; repair-zone blocks
  are treated as zone-boundary content during decode walk.

Repair action (decode intent repair via separate task type):
- Repair is a separate operation from Recover.
- RepairApply planning/synthesis logic exists in code (`Decode_ManifestDiscovery.cpp`).
- Repair consumes repair blocks and writes repaired archive blocks; it does not
  unpack files or folders.
- Repair can synthesize missing archives and zero-fill to expected size.
- Repair request has `mAggressive` policy:
  - `false`: keep an already-valid target block and skip overwrite.
  - `true`: always overwrite target block with repaired block payload.

------------------------------------------------------------------------------
7) CURSORS AND PAUSE/RESUME COMPLEXITY
------------------------------------------------------------------------------

Current cursor families:
- Bundle: discovery, archive packing, repair packing, finalizing headers
- Decode: header bootstrap, discovery, inspection, archive decode, repair apply

Decode logical-record decoder explicitly tracks:
- mode stage (`path_length`, `path_bytes`, `type_flag`, `file_size`, `content_bytes`)
- partial scalar byte buffers (`path_length_le[2]`, `file_size_le[8]`)
- bytes used counters for those scalars
- current path, current output paths (`$WRITING_`, final, `$PARTIAL_`)
- current write stream and bytes written

This means current decode cursor already supports mid-scalar and mid-record pause/resume.

Bundle logical-record encoder simplification:
- It intentionally avoids starting a fixed-width scalar unless it fits in current payload
  capacity (`path_length` and `file_size` boundary checks).
- This reduces scalar-split complexity on pack side.

------------------------------------------------------------------------------
8) ERROR MODEL
------------------------------------------------------------------------------

8.1 Implemented structural format error codes (`MemoryLayoutErrorCode`)
- `kNone`
- `kNullBuffer`
- `kBufferTooSmall`
- `kMagicMismatch`
- `kUnsupportedVersion`
- `kInvalidBooleanByte`
- `kInvalidDirtyState`
- `kInvalidSectionType`
- `kIntegerOutOfRange`
- `kInvalidArgument`

8.2 Runtime decode error markers (`decode_error_kind` info values)
Observed in `Decode_ArchiveDecode.cpp`:
- `block_missing`
- `block_bad_checksum`
- `file_name_error`
- `file_data_error`
- `file_closed_partial`
- `file_discarded_name_boundary`
- `empty_folder_name_error` (legacy empty-folder section decode compatibility)
- `preview_record_skip_started`
- `preview_record_skip_finished`

8.3 Draft public error-family design (not fully implemented)
From `spec.txt`: canceled, invalid request, file system, crypt, family discovery,
header validation, block integrity, record parse, unsafe path, size limit,
recovery exhausted, internal.

Current gap:
- No single unified public error taxonomy emitted end-to-end yet.
- Engine terminal states still rely mostly on phase log strings and task disposition.

------------------------------------------------------------------------------
9) CANCEL POLICY: INTENDED VS CURRENT
------------------------------------------------------------------------------

Intended rule (draft): finish current user file, then honor cancel, then finalize safely.

Current:
- Cancel is global pending flag (`mIsCancelPending`) accepted at engine level.
- Task-level defer logic differs by action/phase.
- Bundle/decode often honor at block boundary, not guaranteed user-file boundary.
- Decode can abort current output file and promote `$PARTIAL_` on cancel/error.
- Bundle finalizes headers with canceled dirty state when applicable.

Conclusion:
- Contract is partially implemented, not fully unified across Bundle/Decode/Repair.

------------------------------------------------------------------------------
10) UI EVENTS, CHECKPOINTS, AND LOG FLOW
------------------------------------------------------------------------------

10.1 Transport
- Engine publishes events through command bus.
- AppShell listens and drains command bus on main queue.
- Engine still advances via high-frequency heartbeat timer (`240 Hz` target).

10.2 Runtime-event capture
- Default verbose runtime capture is off.
- Verbose checkbox toggles `SetCaptureVerboseRuntimeEvents`.
- Runtime events can also be forced on for blocking checkpoint kinds.

10.3 Checkpoints
- Engine supports blocking checkpoint kinds and decision commands.
- AppShell currently does not configure blocking checkpoint kinds.
- Practical result: checkpoint flow is present in engine but mostly dormant in UI.

10.4 Runtime-event scrubbing
- Before runtime events leave bundle/decode contexts:
  - transfer tracking updates started/completed file/archive sets
  - path/file/name keys are scrubbed out
  - label is replaced with runtime-kind label
- This is a major observability/privacy tradeoff: good for path redaction,
  but loses file identity at UI/runtime-event level.

10.5 Log rendering
- AppShell appends log lines for action/cancel/checkpoint/log events.
- Runtime events are currently ignored in AppShell apply switch (no direct line output).

------------------------------------------------------------------------------
11) DEBUG LOG AUTO-SCROLL (CURRENT IMPLEMENTATION)
------------------------------------------------------------------------------

`HomeLogView` behavior:
- Tracks `_autoScrollPinnedToBottom`.
- If user is near bottom (<= 12 px), append sticks to bottom.
- Uses immediate `scrollToBottom` plus deferred main-queue scroll request.

Observed/possible issue under bursty append:
- Large first-burst updates can temporarily unset pinned state before deferred scroll,
  so initial job-start burst may not always stick-to-bottom unless user scrolls down.

------------------------------------------------------------------------------
12) OVERWRITE / DANGLING / JUNK-ON-DISK RISKS
------------------------------------------------------------------------------

12.1 Destination clearing risk
- `ClearDirectory` uses recursive `remove_all`.
- Wrong destination path + Clear choice can wipe unrelated content in that folder tree.

12.2 Merge overwrite risk (bundle)
- In merge mode, archive output paths are deterministic (`prefix + index + .PBTR`).
- Existing files with same names can be truncated by `OpenWriteStream("wb")`.
- No per-file overwrite prompt matrix currently.

12.3 Partial/debris risk (decode)
- Decode writes visible `$WRITING_` files then renames to final.
- On data-boundary failure/cancel it attempts rename to `$PARTIAL_`.
- If rename fails, `$WRITING_` files can remain as debris.

12.4 Repair archive growth behavior
- Repair apply can synthesize missing archives and append zeros to expected size.
- It does not shrink oversized existing files; can leave larger-than-expected files.

12.5 Path escape residual risk
- Relative-path checks block obvious traversal strings.
- Decode output now enforces a destination-root fence before directory/file materialization.
- If a resolved write path would escape destination root (including via symlink topology),
  decode fails that record instead of writing outside destination.

------------------------------------------------------------------------------
13) FRAGMENTATION / COMBINE-SPLIT CLEANUP MAP
------------------------------------------------------------------------------

Known fragmentation from scope shifts:

- Dual execution stacks:
  - Task-based engine path is active (`TaskBundle/TaskDecode/...`).
  - Older Director/Execution wrappers still exist (`Bundle_Director`, `Decode_Director`,
    `*_Execution`) and appear largely unused by current engine.

- Relay/batch docs vs runtime:
  - Relay point contract is documented in `relay_points_spec.md`,
    but runtime still uses heartbeat-driven stepping and direct event emission.

- Preview-manifest accounting:
  - `BundleMemoryPlanning` computes preview bytes.
  - `BundleArchiveManifest` overwrites preview byte count from
    `mPreviewManifestPayload.size()`, which is currently never populated.
  - Can cause accounting/log inconsistency.

- UI runtime-event visibility:
  - Runtime events are generated but currently dropped in AppShell display path.
  - Verbose toggle therefore affects capture pipeline more than operator visibility.

------------------------------------------------------------------------------
14) MISSING HOLES / PROPOSED DECISIONS
------------------------------------------------------------------------------

Proposed finalization decisions to close active ambiguity:

1. Promote explicit "batch contract" into engine core:
   - replace time-budget heartbeat loop semantics with batch-step semantics.
   - keep heartbeat only as compatibility transport wrapper.

2. Standardize cancel boundary by action:
   - Bundle/Decode/Repair should all expose same explicit "safe boundary reached" signal.
   - Default policy: finish current logical user file unless hard integrity fault.

3. Make checkpoint policy usable:
   - configure default blocking checkpoint kinds at AppShell startup.
   - render runtime/checkpoint events in log panel with compact formatting.

4. Resolve merge overwrite policy:
   - choose: strict no-overwrite in merge (auto-suffix), or explicit overwrite mode.
   - document hidden-file and nested conflict behavior.

5. Unify error taxonomy:
   - map structural/runtime/phase failures into stable public families + internal specifics.
   - include deterministic precedence for emitted first failure.

6. Reduce stale code surface:
   - retire or clearly mark unused Director/Execution paths.
   - keep one action-execution architecture to cut drift risk.

------------------------------------------------------------------------------
15) QUICK REFERENCE TABLES
------------------------------------------------------------------------------

Progress stages (`ProgressStageV2`):
- Idle, Preflight, HeaderBootstrap, Discovery, Inspection, MemoryPlanning,
  DeriveCipherMaterial, AssembleCipherStack, ArchiveManifest,
  FolderPacking (legacy stage value, unused in current bundle workflow),
  ManifestDiscovery, ArchivePacking, ArchiveDecode, RepairPacking,
  FinalizingHeaders, Finalize, Compare, RepairApply.

Log levels:
- Info, Warning, Error.

Runtime event kinds:
- Bundle: archive/file/folder/manifest item start/finish, block start/finish,
  encryption finished, repair block start/finish, archive header written/finalized,
  discovery scanned.
- Decode: archive/block/file/folder/manifest item start/finish, decryption start/finish,
  archive/header/block-header events, discovery scanned, inspection scanned,
  decode error, skip jump.
- Repair: archive start/finish/header written, file created/resized,
  repair block start/finish/matched/unmatched.

------------------------------------------------------------------------------
END OF UNIFIED SPEC
------------------------------------------------------------------------------
*/
