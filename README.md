# PeanutButterCoco Technical README

This document is a technical description of the current PBTR archive implementation in this repository.

Scope:

- on-disk archive format
- recovery header contract
- temporary archive and temporary output file behavior
- repair block design
- bundle / decode / recover execution flow

Source-of-truth rule:

- implemented code under `PeanutButterArchiver/Code/*` wins over older prose docs
- this README is written against the current implementation, not against stale draft specs

Current format generation target:

- archive format version written by the bundle path: `2`
- section checksum kind written by the bundle path: `0` (`SHA-256`)
- archive suffix: `.PBTR`

## 1. Core Constants And Terms

Terminology:

- archive family: the full multi-file output set that represents one bundle job
- archive file: one `.PBTR` file inside that family
- block: one fixed-size section-bearing archive unit after the 64-byte archive header
- non-repair zone: preview blocks followed by main data blocks
- repair zone: repair-copy blocks appended after the non-repair zone
- logical record: one serialized file, folder, or reference entry inside the section payload stream

Current constants from code:

- `ArchiveHeaderV2`: `64` bytes
- `SectionHeaderV2`: `96` bytes
- default archive block bytes: `1,044,480`
- default section payload bytes: `1,044,384`
- maximum path length: `16,384`
- default bundle request block count per final archive: `100`
- layout maximum archive count: `1,048,576`
- layout maximum blocks per archive: `2,048`
- deep-recover temp archive capacity: `1,000` blocks per temp file

Numeric encoding:

- fixed-width integers are little-endian
- packed integers are also little-endian:
  - packed uint24 = 3 bytes
  - packed uint48 = 6 bytes

## 2. Final `.PBTR` Family Layout

An archive file is:

1. one plaintext `ArchiveHeaderV2`
2. zero or more fixed-width archive blocks

Each archive block is:

1. one `SectionHeaderV2`
2. one fixed-width payload region of `archive_block_bytes - 96`

For the default layout:

- file bytes = `64 + (block_count * 1,044,480)`
- each block = `96-byte section header + 1,044,384-byte payload`

Family ordering is global, not per-file-type:

1. preview manifest blocks
2. main archive data blocks
3. repair blocks

Block placement rules:

- final bundle output uses `BundleRequestV2.mBlockCount` blocks per archive, default `100`
- the last archive in the family can be shorter
- each section header stores both its archive-local coordinates and family-wide counts

## 3. Archive Header Spec (`ArchiveHeaderV2`)

`ArchiveHeaderV2` is always `64` bytes and is always stored unencrypted at offset `0`.

### 3.1 Byte Layout

| Offset | Size | Field | Type | Meaning |
| --- | ---: | --- | --- | --- |
| `0..7` | 8 | `mMagic` | `u64` | magic constant |
| `8` | 1 | `mArchiveFormatVersion` | `u8` | writer emits `2` |
| `9` | 1 | `mCipherVersion` | `u8` | cipher implementation version |
| `10` | 1 | `mExpanderVersion` | `u8` | password-expander implementation version |
| `11` | 1 | `mDirtyState` | `u8` | provisional/final status |
| `12` | 1 | `mIsEncrypted` | `u8` | `0` or `1` |
| `13` | 1 | `mCipherProfile` | `u8` | encryption strength preset |
| `14` | 1 | `mExpanderProfile` | `u8` | table/expander strength preset |
| `15` | 1 | `mReserved0` | `u8` | currently file-count mod 256 |
| `16..21` | 6 | `mArchiveIndex` | packed `u48` | 0-based archive index inside family |
| `22..27` | 6 | `mArchiveCount` | packed `u48` | total archive file count |
| `28..33` | 6 | `mBlockCountMain` | packed `u48` | total main data block count across family |
| `34..39` | 6 | `mReservedCount0` | packed `u48` | currently written as `0` |
| `40..45` | 6 | `mBlockCountPreview` | packed `u48` | total preview block count across family |
| `46..51` | 6 | `mBlockCountRepair` | packed `u48` | total repair block count across family |
| `52..59` | 8 | `mArchiveFamilyId` | `u64` | family identifier |
| `60..63` | 4 | `mReserved1` | `u32` | low byte currently folder-count mod 256 |

Magic details:

- numeric value: `0x5045414E55544254`
- raw on-disk bytes, because the field is little-endian: `54 42 54 55 4E 41 45 50`

### 3.2 Dirty State Values

| Value | Enum | Meaning |
| ---: | --- | --- |
| `0` | `kInvalid` | provisional/incomplete header |
| `1` | `kFinishedWithCancel` | finalized after cancel |
| `2` | `kFinishedWithError` | finalized with error semantics |
| `3` | `kFinishedWithCancelAndError` | defined, but not the normal bundle final state |
| `4` | `kFinished` | clean finalized bundle |

Bundle write behavior:

- archive packing writes provisional headers first with `mDirtyState = 0`
- finalizing-headers phase patches each archive header in-place to:
  - `4` on normal completion
  - `1` when cancel was observed and finalization was requested

### 3.3 Validator Rules

`ValidateArchiveHeader(...)` currently enforces:

- `mMagic` must equal `0x5045414E55544254`
- `mArchiveFormatVersion` must be non-zero
- `mIsEncrypted` must be `0` or `1`
- `mDirtyState` must be one of `0..4`

Important implementation note:

- the reader does not require `mArchiveFormatVersion == 2`
- the writer does emit `2`

### 3.4 Current Semantic Reuse Of Reserved Fields

- `mReserved0` stores `file_count % 256`
- `mReserved1 & 0xFF` stores `folder_count % 256`
- `mReservedCount0` is written as zero

## 4. Section Header Spec (`SectionHeaderV2`)

`SectionHeaderV2` is always `96` bytes.

Important storage rule:

- for `preview_manifest` blocks, the section header is stored plaintext
- for `archive_data` and `repair_data` blocks in an encrypted family, the entire block is sealed, so the on-disk bytes at the section-header position are ciphertext
- the layout below describes the plaintext section header after unsealing

### 4.1 Byte Layout

| Offset | Size | Field | Type | Meaning |
| --- | ---: | --- | --- | --- |
| `0..31` | 32 | `mCheckSum` | 32 raw bytes | SHA-256 digest over payload + metadata |
| `32..39` | 8 | `mSkipRecord` | `SkipRecordV2` | logical resync pointer |
| `40..43` | 4 | `mRepairRecord` | `RepairRecordV2` | repair target pointer |
| `44` | 1 | `mCheckSumKind` | `u8` | must currently be `0` |
| `45` | 1 | `mSectionType` | `u8` | preview/data/repair |
| `46` | 1 | `mSectionFlags` | `u8` | must currently be `0` |
| `47..50` | 4 | `mPayloadBytesUsed` | `u32` | meaningful bytes in payload region |
| `51..54` | 4 | `mArchiveFileCount` | `u32` | total family archive count |
| `55..58` | 4 | `mArchiveBlockCount` | `u32` | block count in this archive file |
| `59..62` | 4 | `mArchiveIndex` | `u32` | owner archive index |
| `63..66` | 4 | `mBlockIndex` | `u32` | owner block index inside that archive |
| `67..72` | 6 | `mBlockCountMain` | packed `u48` | total main block count |
| `73..78` | 6 | `mBlockCountPreview` | packed `u48` | total preview block count |
| `79..84` | 6 | `mBlockCountRepair` | packed `u48` | total repair block count |
| `85..92` | 8 | `mArchiveFamilyId` | `u64` | family identifier |
| `93..95` | 3 | `mReserved` | raw bytes | reserved, not checksummed |

### 4.2 Section Type Values

| Value | Enum | Meaning |
| ---: | --- | --- |
| `0` | `kArchiveData` | main payload/data zone |
| `1` | `kPreviewManifest` | preview manifest zone |
| `3` | `kRepairData` | repair-copy zone |

Value `2` is not accepted by the validator.

### 4.3 Validator Rules

`ValidateSectionHeader(...)` currently enforces:

- `mCheckSumKind == 0`
- `mSectionType` is one of `0`, `1`, or `3`
- `mSectionFlags == 0`
- `mPayloadBytesUsed <= section_payload_bytes` when non-zero

### 4.4 Checksum Material

`ComputeSectionCheckSum(...)` hashes:

1. the entire fixed payload span, not just `mPayloadBytesUsed`
2. these header fields, in this order:
   - `mSkipRecord`
   - `mRepairRecord`
   - `mCheckSumKind`
   - `mSectionType`
   - `mSectionFlags`
   - `mPayloadBytesUsed`
   - `mArchiveFileCount`
   - `mArchiveBlockCount`
   - `mArchiveIndex`
   - `mBlockIndex`
   - `mBlockCountMain`
   - `mBlockCountPreview`
   - `mBlockCountRepair`
   - `mArchiveFamilyId`

Not checksummed:

- `mCheckSum` itself
- `mReserved[3]`

Implication:

- block writers zero-pad unused payload bytes before checksum generation
- checksum validation is over the full padded block payload, not over a variable-length prefix

## 5. Skip Record Spec (`SkipRecordV2`)

`SkipRecordV2` is `8` bytes:

| Offset | Size | Field | Type |
| --- | ---: | --- | --- |
| `0..2` | 3 | `mArchiveIndex` | packed `u24` |
| `3..4` | 2 | `mBlockIndex` | `u16` |
| `5..7` | 3 | `mByteIndex` | packed `u24` |

Meaning:

- points to the first payload byte of the next logical-record boundary
- target coordinates are archive index, archive-local block index, and payload byte offset

Writer behavior:

- ordinary data blocks may carry a valid forward skip target
- repair-copy blocks inherit the source block's skip record
- blocks without a meaningful skip target are not zeroed; they get a deliberately invalid out-of-range value

Reader behavior:

- recover mode treats all-zero skip records as absent
- recover only applies a skip record after additional target validation

## 6. Repair Record Spec (`RepairRecordV2`)

`RepairRecordV2` is `4` bytes:

| Offset | Size | Field | Type |
| --- | ---: | --- | --- |
| `0..1` | 2 | `mArchiveIndex` | `u16` |
| `2..3` | 2 | `mBlockIndex` | `u16` |

Meaning:

- in a repair-copy block, points to the archive-local target block that this repair payload can replace

Writer behavior:

- non-repair blocks get a deliberately invalid out-of-range repair target
- repair-copy blocks get a valid archive-local target for the copied source block

Repair-apply behavior:

- when a repair block is turned back into a normal block, the patched target header resets the repair record to:
  - `mArchiveIndex = 0xFFFF`
  - `mBlockIndex = 0xFFFF`

## 7. Logical Record Stream Format

The section payload is a concatenated stream of typed logical records.

Important stream property:

- scalar fields may cross block boundaries
- the decode state machine explicitly supports mid-scalar and mid-record resume

### 7.1 Record Type Values

| Value | Enum | Meaning |
| ---: | --- | --- |
| `1` | `kManifestFile` | preview-manifest file record |
| `2` | `kManifestFolder` | preview-manifest folder record |
| `3` | `kDataFile` | ordinary file record with content bytes |
| `4` | `kDataFolder` | ordinary folder record |
| `5` | `kDataReference` | symlink/alias/reference record |

### 7.2 Reference Kind Values

| Value | Enum | Meaning |
| ---: | --- | --- |
| `1` | `kSymlink` | symbolic link |
| `2` | `kAlias` | Apple alias descriptor |
| `3` | `kReparsePoint` | recognized by parser, unsupported for materialization |
| `4` | `kHardlink` | recognized by parser, unsupported for materialization |

### 7.3 Common Prefix

Every record begins with:

1. `path_length_le16`
2. `path_bytes[path_length]`
3. `type_flag_u8`

Path rules enforced by the decoder:

- non-empty
- `<= 16,384` bytes
- not absolute/rooted
- no drive-letter root
- no empty path segments
- no `.` segment
- no `..` segment
- no control bytes, `DEL`, or `NUL`

### 7.4 Per-Type Payload Shape

`manifest_folder`:

1. common prefix
2. `preview_placeholder_u8` with fixed value `0`

`manifest_file`:

1. common prefix
2. `preview_placeholder_u8` with fixed value `0`
3. `file_size_le64`

`data_folder`:

1. common prefix

`data_file`:

1. common prefix
2. `file_size_le64`
3. `file_content_bytes[file_size]`

`data_reference`:

1. common prefix
2. `reference_kind_u8`
3. `reference_target_length_le16`
4. `reference_target_bytes[reference_target_length]`

### 7.5 Zone Rules

`preview_manifest` blocks:

- contain `manifest_folder` and `manifest_file` records
- never carry file content bytes
- are stored plaintext even when archive encryption is enabled

`archive_data` blocks:

- contain `data_folder`, `data_file`, and `data_reference` records
- are encrypted when archive encryption is enabled

`repair_data` blocks:

- do not carry a distinct record grammar
- they carry a copied source block payload plus a retargeted section header

### 7.6 Record Ordering

Current bundle implementation sorts records lexicographically by relative path before encoding.

That applies to:

- preview records
- data records
- reference records
- empty-folder records

### 7.7 Reference Record Semantics

Current discovery behavior:

- symlinks and aliases are preserved as `data_reference` records
- they are not dereferenced into ordinary file content
- directory links are treated as references too, because discovery checks `IsSymlink` / `IsAlias` before recursing into directories

Current reference payload conventions:

- symlink targets are stored as safe relative paths inside the source root
- alias targets are stored as descriptors:
  - `r/...` or `r` = destination-root-relative
  - `h/...` or `h` = home-relative
  - `a/...` or `a` = absolute-root-relative

Current decode materialization rules:

- symlink records create symlinks
- alias records try to create an alias file, then fall back to symlink creation
- `reparse_point` and `hardlink` decode paths are parsed but not materialized

## 8. Recovery Header Spec

There is no dedicated `RecoveryHeaderV2` struct in current code.

The repository uses `ArchiveHeaderV2` as the recovery header for deep-recover temp archives.

This README uses the term recovery header to mean:

- a synthetic `ArchiveHeaderV2` written at the start of each `$RECOVER_data_*.dat` temp archive

### 8.1 Recovery Header Layout

Byte layout is exactly the `ArchiveHeaderV2` layout from section 3.

### 8.2 Recovery Header Field Contract

Deep-recover temp archives are created by copying the bootstrap header and then patching these fields:

| Field | Recovery value |
| --- | --- |
| `mDirtyState` | `kFinishedWithError` (`2`) |
| `mIsEncrypted` | `0` |
| `mArchiveIndex` | temp-archive ordinal |
| `mArchiveCount` | expected archive count from bootstrap/manifest discovery |
| `mBlockCountMain` | expected family main block count |
| `mReservedCount0` | `0` |
| `mBlockCountPreview` | expected family preview block count |
| `mBlockCountRepair` | expected family repair block count |

Fields inherited unchanged from bootstrap header:

- `mMagic`
- `mArchiveFormatVersion`
- `mCipherVersion`
- `mExpanderVersion`
- `mCipherProfile`
- `mExpanderProfile`
- `mArchiveFamilyId`
- count-mod bytes in `mReserved0` and `mReserved1`

Meaning:

- temp archives are plaintext containers of already-validated plain blocks
- they preserve family counts and family id so the temp file still behaves like an archive-shaped container

## 9. Temporary Archive Files And Temporary Output Files

This codebase uses two different temporary-file families:

1. deep-recover temp archives
2. visible decode-output temp files

### 9.1 Deep-Recover Temp Archive Root

Recover mode creates a unique directory inside the decode destination:

- preferred root name: `$RECOVER`
- collision fallback: `$RECOVER_1`, `$RECOVER_2`, and so on

The path generator is `MakeUniquePath(...)`, so names are unique-by-existence, not by PID or timestamp.

### 9.2 Deep-Recover Temp Archive File Names

Each packed temp archive file is created under that root as:

- `$RECOVER_data_00001.dat`
- `$RECOVER_data_00002.dat`
- and so on

Details:

- numbering is 1-based
- numeric width is fixed at 5 digits
- collisions also get `_N` suffixes from `MakeUniquePath(...)`

### 9.3 Deep-Recover Temp Archive File Format

Each temp archive file is:

1. one synthetic recovery header (`ArchiveHeaderV2`, 64 bytes)
2. up to `1000` fixed-width archive blocks

Stored blocks are plaintext validated blocks:

- readable preview blocks are staged as-is
- readable encrypted data blocks are unsealed first, then staged plaintext
- repair blocks are not staged into a regular slot as raw repair blocks
- instead, when a repair block can satisfy a missing regular slot, recover builds the patched plain target block and stages that plain block

Write rule:

- block `n` is written at `64 + (n * archive_block_bytes)`

Lifecycle:

- the temp root is cleared before a new deep-recover scan begins
- sealed temp archives can be deleted early once all live block references are consumed
- cleanup tries to remove the whole temp root at the end

### 9.4 Preserve Directory Inside Recover Temp Root

Recover mode can do a temp-backed replay pass over staged blocks.

Before replay begins it preserves current user-visible outputs under:

- `$RECOVER.../$PRESERVE`

Rules:

- existing destination entries, except the recover temp root itself, are renamed into `$PRESERVE`
- transient `$WRITING_*` files inside preserved content are pruned
- after replay, preserved outputs that were not replaced can be restored back into the destination

### 9.5 Visible Decode Output Temp Files

When the logical record decoder materializes a file, it reserves three output paths:

- final path
- visible write-in-progress path: `$WRITING_<leaf>`
- partial path: `$PARTIAL_<leaf>`

Rules:

- bytes stream into `$WRITING_<leaf>`
- on success, that path is renamed to the final path
- on damage/cancel/continuation failure, it tries to rename to `$PARTIAL_<leaf>`
- if the preferred `$PARTIAL_` name already exists, it resolves a no-overwrite alternative path
- if rename fails, `$WRITING_` debris can remain on disk

## 10. Repair Blocks

Repair blocks are exact block copies encoded as normal section blocks in the repair zone.

They are not parity blocks and they are not erasure-code shards.

### 10.1 Coverage Policy

Coverage presets:

- `20%`
- `40%`
- `60%`
- `80%`

Current selection formula:

- eligible source blocks = main-zone blocks only
- preview blocks are excluded
- selected repair block count = `ceil(eligible_source_blocks * coverage_percent / 100)`

Selection order:

- first eligible main blocks in family order are chosen
- this is front-of-main coverage, not random spread and not round-robin

### 10.2 Repair Destination Placement

Repair blocks are appended after all preview and main blocks.

Placement order:

1. archive order ascending
2. within each archive, local block order ascending
3. repair blocks begin immediately after that archive's non-repair source blocks

### 10.3 How A Repair Block Is Built

Bundle repair packing does not read back finished archive bytes and copy them raw.

Instead it:

1. deterministically rebuilds the source block payload from the logical-record stream
2. computes the same skip record the source block would have carried
3. writes a new section header for the repair-destination location
4. sets:
   - `mSectionType = repair_data`
   - `mRepairRecord = {source_archive_index, source_local_block_index}`
   - `mArchiveIndex` and `mBlockIndex` = repair block's actual destination coordinates
5. recomputes checksum
6. seals the block if the family is encrypted

Effects:

- repair blocks preserve the copied payload bytes
- repair blocks preserve the copied source skip record
- repair blocks do not preserve the source section type in the header; the stored type is always `repair_data`

### 10.4 How Repair Blocks Are Consumed By `repair`

The separate repair action:

1. scans the repair zone only
2. validates each repair block
3. requires:
   - readable block
   - valid checksum
   - matching family id
   - `section_type == repair_data`
   - target pointer inside the non-repair zone
4. determines what the target section type should be:
   - preview manifest if the target is in the global preview prefix
   - archive data otherwise
5. rebuilds a patched target block from the repair block by:
   - replacing section type with the resolved target type
   - resetting repair record to `0xFFFF/0xFFFF`
   - recomputing checksum
   - resealing if the family is encrypted
6. overwrites the target block in the destination archive

Overwrite policy:

- `mAggressive = false`: skip overwrite if the existing target block already validates
- `mAggressive = true`: always overwrite with the repaired payload

### 10.5 How Repair Blocks Interact With `recover`

Normal recover decode does not walk the repair zone as if it were ordinary file payload.

Important distinction:

- recover is an unpack-with-salvage workflow
- repair is a block-replacement workflow

Current recover behavior:

- normal archive decode treats repair-zone blocks as zone-boundary content, not as live file payload
- deep-recover healing scan can still inspect a repair block
- if that repair block points to a missing regular slot, recover can synthesize the patched plain target block and stage that into temp storage

So:

- recover can benefit from repair blocks indirectly during healing
- recover does not simply stream repair-zone payload into files

## 11. Process: `bundle`

Phase order:

1. `Preflight`
2. `Discovery`
3. `MemoryPlanning`
4. `DeriveCipherMaterial` if encryption is enabled
5. `AssembleCipherStack` if encryption is enabled
6. `ArchiveManifest`
7. `ArchivePacking`
8. `RepairPacking` if repair is enabled
9. `FinalizingHeaders`

Implementation details by phase:

`Discovery`

- walks the source tree
- collects ordinary files
- collects empty folders
- preserves symlinks and aliases as reference records
- does not dereference those links into ordinary content bytes

`MemoryPlanning`

- computes preview bytes, main logical bytes, and family block counts
- chooses repair-copy sources
- computes archive count and archive file paths
- computes the family id from source stem, request settings, counts, and discovered records

`ArchiveManifest`

- currently acts mostly as a summary/accounting phase
- preview blocks are still encoded on demand during archive packing

`ArchivePacking`

- writes a provisional archive header first
- writes preview blocks, then main data blocks
- preview blocks are plaintext
- main data blocks are sealed when encryption is enabled
- unreadable source file chunks are zero-filled so packing can continue deterministically

`RepairPacking`

- appends repair-copy blocks at the end of the family

`FinalizingHeaders`

- patches each archive header in-place from provisional `dirty_state = invalid`
- final value becomes:
  - `finished`
  - or `finished_with_cancel` if cancel was observed and finalization was requested

Cancel behavior:

- bundle observes cancel at block boundaries
- it is not guaranteed to finish the current user file before honoring cancel
- if finalization is requested after cancel, partially written archives still get finalized headers

## 12. Process: `decode` Without `recover`

Decode intent:

- `DecodeIntentV2::kUnbundle`

Phase order:

1. `Preflight`
2. `HeaderBootstrap`
3. `Discovery`
4. `DeriveCipherMaterial`
5. `AssembleCipherStack`
6. `Inspection`
7. `ManifestDiscovery`
8. `ArchiveDecode`
9. `Finalize`

Detailed behavior:

`HeaderBootstrap`

- resolves a bootstrap archive and reads a real `ArchiveHeaderV2`

`Discovery`

- scans candidate archive files
- matches either by readable header or by bootstrap filename template
- sorts discovered archives by archive index

`Inspection` / `ManifestDiscovery`

- refines expected archive count and block counts using section-header truth

`ArchiveDecode`

- reads blocks in expected family order
- decrypts non-preview blocks when required
- validates section header and section checksum
- feeds payload into the logical record decoders
- materializes folders, files, symlinks, and aliases in the destination

Damage policy in unbundle mode:

- `ShouldStopAfterFirstDamagedBlock(...)` is true
- the current output file/record is aborted or promoted to partial
- the archive-decode phase finalizes after the first damaged block instead of entering salvage mode

Practical effect:

- `decode` without recover is strict
- it may leave partial files, but it does not attempt skip-record resync or temp-backed healing

## 13. Process: `decode` With `recover`

Decode intent:

- `DecodeIntentV2::kRecover`

Recover uses the same phase list as ordinary decode, but behavior diverges in bootstrap and archive decode.

### 13.1 Bootstrap Differences

Recover can synthesize a bootstrap header when no readable archive header is available.

Current synthetic-bootstrap rules:

- `dirty_state = finished_with_error`
- `is_encrypted` comes from the user request
- archive count is guessed from the number of `.PBTR` files
- block counts start at zero and are refined later

### 13.2 Decode Differences

Recover starts in the normal optimistic decode walk, then escalates only when needed.

Escalation ladder:

1. optimistic ordered decode
2. pessimistic salvage walk after damage
3. skip-record resync when a valid anchor exists
4. deep-recover healing scan and temp-backed replay when the decoder must wait for staged blocks

### 13.3 Pessimistic Salvage Walk

When recover sees:

- unreadable block
- decrypt failure
- checksum failure
- invalid section header
- logical section-index jump
- continuation mismatch

it can:

- close the current file as partial
- switch to pessimistic mode
- continue from the next recoverable boundary instead of terminating immediately

### 13.4 Skip-Record Resync

Recover can use skip records in two ways:

- local resync anchor inside the current block
- forward jump to a later block and payload offset

Conditions:

- only in recover mode
- only when the target validates as plausible
- either as a validated forward jump, or as a validated in-block/local anchor inside the current payload span

Effect:

- the decoder can reopen parsing at a logical-record boundary instead of blindly continuing from byte zero of the next block

### 13.5 Deep-Recover Healing Scan

When recover must wait for missing or damaged content, it can enter healing mode.

Healing mode:

1. creates the `$RECOVER...` temp root
2. scans all discovered archives
3. validates readable blocks
4. stages valid regular blocks into temp archives
5. inspects repair blocks and, when useful, stages patched plain target blocks into temp archives
6. seals archives or archive tails once enough staged blocks exist for resumed decode

Two resume styles exist:

- sealed-healing resume from temp-backed blocks
- full output replay from staged temp archives

### 13.6 Output Replay Behavior

Before a full replay pass:

- recover stashes current visible outputs into `$PRESERVE`
- clears the destination except for the recover temp root
- replays output from staged temp archives

After replay:

- missing preserved outputs can be restored back from `$PRESERVE`
- temp archives and preserve state are cleaned up best-effort

### 13.7 What Recover Does Not Do

Recover does not:

- mutate the original archive family on disk
- permanently repair archive files in place
- treat repair-zone blocks as ordinary file payload during the normal decode walk

That is the job of the separate `repair` action.

## 14. Testing Plan

The current repository tests the format at three layers:

1. block/header packing correctness
2. clean round-trip decode correctness
3. corrupted-media strict-decode and recover-decode behavior

### 14.1 Structural Bundle Verification

The bundle path is checked against a mock in-memory builder before any corruption is introduced.

What is verified:

- payload bytes for every block
- section type for every block
- archive index and local block index
- preview/main/repair block counts
- repair-record target coordinates
- skip-record archive/block/byte coordinates

Important property:

- this is not just a coarse "can it decode" test
- `BundleVerify` checks that real output matches the mock model at block granularity, including skip records and repair records

### 14.2 Baseline Happy-Flow Round Trip

The normal round-trip path is:

1. build a job with a synthetic file/folder set
2. bundle it
3. decode it without corruption
4. compare decoded output with the source model

Purpose:

- prove that the archive family is internally consistent before any mutation campaign begins
- produce the reference archive family later used by corruption tests

### 14.3 Corrupt-Media Round Trip

After happy-flow succeeds, the same job is exercised in two damaged-media modes:

1. strict `decode` without recover
2. `decode` with recover enabled

Current harness shape:

- strict decode is expected to stop early on the first damaged block
- recover decode is compared against the mock salvage oracle
- deterministic regression suites cover fixed failures
- randomized suites vary geometry and corruption patterns

Current randomized medium matrix:

- `RoundTripTests_Medium` runs `50,000` jobs
- payload bytes per block vary from `1..256`
- blocks per archive vary from `1..256`
- preview is toggled on/off
- repair coverage is chosen from `0`, `20`, `40`, `60`, `80`
- up to `8` mutations can be generated for a job

Broader regression coverage:

- `RoundTripTests`
- `RoundTripTests_Regression`
- `RoundTripTests_Nonfatal`
- `RoundTripTests_Dragon`
- `RoundTripTests_Unglorified`
- `CancelTests_Bundle`

Those suites are where many of the hand-picked swapped-block and archive-loss regressions live.

## 15. Mutation Types And Failure Semantics

In this section, "missing files" means missing final archive files such as `bdl_7.PBTR`, not missing source-side user files.

### 15.1 Mutation Kinds

Current explicit mutation enum:

| Mutation | Meaning |
| --- | --- |
| `kMangleBlock` | corrupt one physical block |
| `kDeleteBlock` | treat one logical block slot as gone |
| `kSwapBlocks` | exchange two intact blocks |
| `kDeleteArchive` | remove one whole archive file |
| `kMangleArchive` | corrupt one whole archive file |

Important test-harness detail:

- in the real mutation path, `kDeleteBlock` is implemented by mangling the block bytes on disk
- in the mock model, later block indices for that archive are adjusted so the test still models a genuinely missing logical slot
- similarly, `kDeleteArchive` and `kMangleArchive` are treated equivalently by the mock salvage model: all blocks in that archive become unusable

### 15.2 Why Swapped Blocks Are Difficult

Swapped blocks are harder than simple corruption because both blocks can remain internally valid.

What survives a swap:

- payload bytes
- section checksum
- section header coordinates
- repair record
- skip record

What breaks:

- physical order no longer matches logical order
- the ordered decode walk encounters a section-index discontinuity
- open-file continuation state may no longer match the next physical block

Implication:

- strict decode usually stops because the next physical block is not the next logical block
- recover has a better chance because the block headers still identify the blocks' intended logical coordinates, so valid blocks can be regrouped into consecutive logical runs

### 15.3 Why Missing Blocks Are Difficult

A missing block creates a hole in the logical byte stream.

That hole is especially painful when the lost block held:

- the beginning of a record
- the middle of a path scalar
- the type flag
- a file-size scalar
- the middle of file-content bytes for a large file

Implication:

- blocks after the hole may still be individually valid
- but the decoder cannot safely interpret later bytes until it reaches a trusted logical-record boundary
- if the missing block had a repair copy, that exact block can be restored independently
- if it did not, salvage depends on finding a later skip-record anchor

### 15.4 Why Corrupt Blocks Are Difficult

Corrupt blocks are similar to missing blocks from the parser's perspective, but the failure signal is different.

Typical failure surfaces:

- decryption/seal failure
- invalid section header
- checksum mismatch
- payload continuation mismatch

Implication:

- the block is present physically, but not trustworthy logically
- recover must discard it as a candidate source block unless a repair copy can replace it
- once discarded, the remaining problem becomes the same as a missing-slot problem: the byte stream has a hole

### 15.5 Why Missing Archive Files Are Difficult

Losing one `.PBTR` file is not one failure, but a contiguous range failure.

A missing archive file removes:

- the archive header for that file
- every block stored in that file
- every preview/main/repair role those blocks carried

Implication:

- bootstrap may have to be synthesized if early headers are gone
- a whole run of logical blocks disappears at once
- if the missing archive contained early main blocks, later surviving blocks may have no usable front-of-stream entry point
- repair coverage can only help for source blocks that were actually copied into the repair zone

## 16. Block Robustness And Limitations

### 16.1 What Makes The System Robust At Block Granularity

From a block perspective, the format is deliberately redundant in metadata.

Every block carries enough information to be handled independently:

- family id
- total preview/main/repair counts
- archive index
- archive-local block index
- section type
- section checksum
- optional skip record
- optional repair record

Practical effect:

- a scanner can validate one block without trusting its neighbors
- a valid block can be identified even when found out of physical order
- repair targets are block-addressed, not file-addressed
- deep recover can stage blocks one at a time into temporary archive-shaped containers

### 16.2 Every Block Is Individually Recoverable

At the storage-layer level, every block is individually recoverable.

More precisely:

- if the original block survives, it can be recovered independently of neighboring blocks
- if the original block is gone but an exact repair copy exists for that slot, that one block can be reconstructed independently
- if neither survives, that block cannot be rebuilt

The important design point is that no surviving block depends on adjacent blocks to be identified, validated, or staged.

That statement is true in this precise sense:

- each block is independently locatable by archive/header geometry
- each block is independently validated by its own checksum and metadata
- each block is independently identified by its logical coordinates
- each covered main block can be independently replaced by its exact repair copy
- each surviving or reconstructed block can be staged into temp storage without reconstructing unrelated blocks first

Important limit:

- block-level recoverability does not automatically imply record-level decodability
- the block may be recovered as a block object, yet still be unusable for file reconstruction until the decoder finds a safe logical-record entry point

### 16.3 Limits When No "File Fronts" Are Available

The code does not use the term "file front", but it is a useful way to describe the problem.

In practice, a usable front means a place where the decoder can start parsing with known grammar state, such as:

- the first main block in the family
- a validated skip-record boundary that lands on a logical-record start

Without such a front, a run of valid blocks may still be unusable.

Why:

- the record grammar is a streaming grammar
- path length, path bytes, type flag, file size, reference payload, and file-content bytes can all span blocks
- a later block can therefore begin in the middle of:
  - a path string
  - a file-size scalar
  - a reference target
  - file-content bytes from an already-open file

When no front is available, recover may know that the block bytes are good, but it does not know how to interpret the first byte of the run.

Practical consequences:

- the system can preserve and stage the block
- the system can often use that block later if a valid front is found upstream
- but the system cannot safely invent parser state
- some file tails or entire files can remain unrecoverable even though later blocks themselves survived

Important limitation of preview data here:

- preview blocks can describe names and file sizes
- they do not provide main-data parser state
- they do not let recover safely start in the middle of a `data_file` content stream

## 17. Skip Records In Detail

Skip records are the bridge between block-level survival and record-level re-entry.

They do not mean "skip damaged bytes blindly."

They mean:

- "the next known logical-record boundary reachable from this block starts at these coordinates"

### 17.1 How Skip Records Are Generated

Current writer behavior:

1. build the full ordered list of main-zone logical records
2. compute each record's start byte in the flattened main-data payload stream
3. for each main data block after block `0`, find the first record start byte that is greater than or equal to that block's payload start
4. convert that byte position into:
   - archive index
   - archive-local block index
   - payload byte offset
5. store that as the block's skip record

Important consequences:

- preview blocks do not get valid skip records
- the first main data block does not get a valid skip record
- a later block can legally point to itself at payload offset `0` if that block begins exactly on a record boundary
- a later block can point forward to another block and offset if the next record boundary is later
- if there is no later-or-equal record boundary, the skip record remains deliberately invalid

### 17.2 Invalid Skip Records

An invalid skip record is not simply "all zeros".

Current writer behavior:

- invalid skip records are written as deliberately out-of-range coordinates
- recover also treats all-zero skip records as absent, but current writers do not rely on zero as the invalid encoding

Reason:

- this avoids conflating "never initialized" with "intentionally no safe resume point"

### 17.3 How Recover Uses Skip Records

Recover uses skip records only after validating that the target is plausible.

Two recover uses exist:

1. local in-block anchor
2. forward jump to a later block

Local anchor case:

- target archive/block matches the current logical block
- target payload offset lies inside the current payload span
- recover can restart parsing inside the current block at that validated boundary

Forward jump case:

- target archive/block is later in logical order
- target payload offset is within range
- target block must be readable or staged
- recover marks the jump as pending and validates the landing block before trusting it

Important recover behavior:

- skip-record resync is recover-only
- strict decode does not use it
- full temp replay arms skip-record resync immediately because replay can start from a repaired run boundary instead of from the pristine beginning of the stream

### 17.4 Why Repair-Block Skip Records Must Match The Original

A repair block is an exact replacement for one specific main block.

That replacement must preserve two things:

1. the payload bytes
2. the logical re-entry semantics of the source block

The second requirement is why the repair block copies the original block's skip record.

If the repair block carried a different skip record, then after recover or repair turns it back into a normal block:

- the block would contain the right bytes
- but the decoder would observe a different "next safe boundary" than the original block provided
- deep recover replay could re-enter at a different location
- salvage output could diverge from what the original block layout intended

So, for repair blocks, "exact copy" effectively means:

- same payload
- same skip anchor semantics
- different section type and destination coordinates
- added repair target pointer

The repository enforces this in two places:

- mock bundle generation copies the source skip record onto the repair block
- `BundleVerify` requires the real packed skip record to match the mock skip record exactly

## 18. Top Advantages And Weaknesses

### 18.1 Top 5 Advantages

1. Block-local validation is strong.
   Every block carries enough metadata and checksum material to be scanned, identified, and trusted independently.

2. Recovery is block-addressed rather than archive-global.
   The engine can salvage or replace one block without reconstructing an unrelated region first.

3. Swapped or out-of-order blocks are still usable if their bytes survive.
   The internal block coordinates travel with the block, so recover can regroup intact blocks by logical order.

4. Skip records provide deterministic re-entry points.
   Recover does not have to guess where the next record begins when a validated boundary is available.

5. Deep-recover staging is non-destructive to the original media.
   Recover can assemble usable temp archives and replay them without rewriting the damaged source family.

### 18.2 Top 3 Weaknesses

1. There is no parity or erasure coding.
   If a main block is lost and no exact repair copy exists for that slot, the block itself is gone.

2. Repair coverage is front-loaded.
   The current planner protects the first eligible main blocks in family order, so later blocks are less likely to have direct repair copies.

3. Record-level salvage is anchor-dependent.
   Valid blocks are not enough by themselves; without a first-block front or a skip-record boundary, the decoder cannot safely reconstruct parser state.

## 19. Summary Of Current Format/Workflow Facts

- final output archives use one plaintext archive header plus fixed-size blocks
- preview, main, and repair zones are distinct and globally ordered
- preview blocks are plaintext; main and repair blocks are sealed when encryption is enabled
- links are preserved as explicit reference records, not dereferenced into ordinary file data
- repair blocks are exact copied block payloads with retargeted headers, not parity shards
- recover mode can use skip records and deep temp archives to salvage more output than strict unbundle mode
- there is no standalone recovery-header struct; recovery temp archives reuse `ArchiveHeaderV2`
