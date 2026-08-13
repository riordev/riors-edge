# Save / State Architecture

Status: design target. Nothing below the Migration section is implemented.
Owner layer: C++ (save formats are a durable-rules concern per `Docs/Architecture.md`).

This document defines what is saved, where it lives, how it survives a crash,
how it versions forward, and what changes when the authority moves to a server.

Every locked master-sheet decision that constrains this design:

- Level cap 50, hard stop. Gear is the entire endgame. **The item table is the
  most valuable thing in the save file** — losing progression costs hours,
  losing items costs the entire game.
- Class selection is permanent per character. This forces multi-character slots:
  the only way to play a second class is a second character.
- Solo is the primary balance target. The save format must work fully offline.
- Respec is Forge-gated; class/core points store stable IDs and ranks, never
  pointers or calculated totals (`7.9 Implementation Rules`).
- Aberrant max 3 equipped / Anomalous max 1 equipped — equip limits are the
  endgame decision, so they must be re-validated on load, not trusted.

---

## 1. Current state (as of this pass)

| Thing | Where it lives today |
|---|---|
| Progression state | `UBreakerSaveGame::Progression` → slot `BreakerSave0`, index 0 |
| Equipped items (8 slots) | `UBreakerSaveGame::EquippedItems` |
| Backpack | `UBreakerSaveGame::BackpackItems` |
| Weapon slot archetypes | `SlotOneArchetype` / `SlotTwoArchetype` |
| Sensitivity / FOV / invert | `GConfig` → `GGameUserSettingsIni`, section `RiorsEdge.Playtest` |
| Save trigger | `ABreakerCharacter::BeginPlay` load, `EndPlay` + class-lock save |
| Version fields | `int32 SaveVersion = 1` on `UBreakerSaveGame`, `FBreakerProgressionState`, `FBreakerItemInstance` |

Known defects in the current shape, in severity order:

1. **Single slot.** `BreakerSave0` is hardcoded. Class is permanent, so one slot
   means one class forever. This is a design contradiction, not a nice-to-have.
2. **No stash, no currency, no account layer.** Everything is per-character by
   accident rather than by decision.
3. **No backup.** A crash during `SaveGameToSlot` truncates the only copy.
   Unreal writes the `.sav` in place. There is no rotation and no checksum.
4. **`EndPlay` is not a crash-safe cadence.** A hard crash or power loss loses
   the entire session. Rift clears, level-ups, and drops all evaporate.
5. **The three `SaveVersion` fields are declared but never read.** There is no
   migration function anywhere. They are currently decoration.
6. **Load does not validate.** `RestoreState` trusts the file. A hand-edited
   save can equip five Anomalous items or set level 90.

---

## 2. Target: the three-tier state model

State is split into three tiers with different lifetimes, different owners, and
eventually different storage.

```
ACCOUNT  (one per player)         → BreakerAccount.sav
   |
   +-- CHARACTER (up to 12)       → BreakerChar_<Guid>.sav
          |
          +-- SESSION (volatile)  → BreakerRun_<Guid>.sav  (crash journal only)
```

### 2.1 ACCOUNT-WIDE

| Data | Rationale |
|---|---|
| Stash (shared item vault) | The whole point of a second character is gearing it. A per-character stash makes alts a fresh grind, which contradicts "gear is the endgame" — gear should be an account asset, characters are builds. |
| Crafting materials / currencies | Same reason. Also removes the "which character is my crafting mule" anti-pattern. |
| Character slot roster (id, name, class, level, playtime, last-played) | Needed to draw the select screen without deserializing every character file. |
| Fragment / story-collectible unlocks (`1.7`) | Fragments unlock *capabilities* (deeper rift access, Forge capability). Re-earning them on every alt is pure repetition with no build expression. EXTENDS: master sheet does not state fragment scope. |
| Anchor / vendor / NPC standing, final-choice epilogue flag | Narrative epilogue only, does not gate content (`8.6`), so account-wide is free. |
| Codex / bestiary / affix-seen ledger | Pure knowledge. |
| Settings (input, sensitivity, FOV, invert, audio, accessibility, camera roll/shake toggles per `5.4`) | Machine-and-account, never per-character. |
| Keybinds | Same. |

### 2.2 PER-CHARACTER

| Data | Rationale |
|---|---|
| Permanent class id | Locked decision. Written **once**, then immutable — see 2.4. |
| Level, XP, unspent Class Points, unspent Core Points | |
| Class node ranks, Core node ranks (stable IDs + ranks) | Per `7.9`. |
| Ability loadout (2 class abilities + 1 ultimate) | |
| Equipped items (8 slots) | |
| Backpack | Character-carried, distinct from account stash. |
| Weapon slot archetypes | |
| Campaign act/quest progress, unlocked fast-travel | |
| Per-character playtime, death count, deepest rift tier | |

### 2.3 The stash-vs-backpack boundary

Backpack is per-character and small (a run's worth of loot). Stash is
account-wide and large. Transfer happens **only inside an Anchor** — the Anchor
is already the diegetic safe ground (`1.2`) and the Forge/vendor location, and
gating transfers there means a rift run cannot mutate account state. That single
rule is what makes the crash journal in §4 tractable: during a rift, account
state is read-only.

Proposed initial sizes (tune after the loot-rate pass, which is blocked on TTK):

| Container | Slots | Note |
|---|---|---|
| Backpack | 60 | Overflow → forced Anchor return or auto-salvage prompt |
| Stash | 300, expandable in 50-slot pages | Expansion is a currency sink, not a power sink |

### 2.4 Immutability rules

Some fields must never change after first write. Enforce in code, not by
convention:

- `CharacterId` (FGuid) — identity.
- `PermanentClass` — locked design decision. `ChoosePermanentClassById` must
  refuse if the field is already set to anything but `None`, and the save path
  must refuse to write a character record whose class differs from the one on
  disk. Today the only guard is in the progression component; it must also exist
  at the save boundary, because that is the layer an edited file attacks.
- `CreatedAtUtc`.

---

## 3. File layout and slot naming

```
Saved/SaveGames/
  BreakerAccount.sav
  BreakerAccount.bak1 .bak2 .bak3
  BreakerChar_<Guid>.sav
  BreakerChar_<Guid>.bak1 .bak2 .bak3
  BreakerRun_<Guid>.sav          (deleted on clean rift exit)
```

- `UBreakerSaveSubsystem` (a `UGameInstanceSubsystem`) owns every read and write.
  Nothing else calls `UGameplayStatics::SaveGameToSlot` — `ABreakerCharacter`
  stops doing it directly. This is the single change that makes everything else
  in this document possible.
- Character slot cap: **12**. Five classes × a build-variant each, plus room.
  Cheap to raise; expensive to lower after players fill them.
- Guid-named files, not indices, so deleting character 3 does not renumber
  characters 4–12 and does not risk a rename losing a file.

### 3.1 Header block

Every `.sav` starts with a small fixed header that can be read without
deserializing the payload, so the character-select screen is fast and a corrupt
payload is detectable before it is trusted:

| Field | Type | Purpose |
|---|---|---|
| `Magic` | uint32 `'BRKR'` | Reject foreign files immediately |
| `FormatVersion` | int32 | Whole-file schema version (see §5) |
| `EngineVersion` | FString | Diagnostics only, never gating |
| `WriteCounter` | uint64 | Monotonic; the newest of a `.sav` and its backups wins |
| `PayloadHash` | uint64 (CityHash64) | Corruption detection |
| `bCleanClose` | bool | False means the process died mid-session |
| `SummaryName / Class / Level / PlaytimeSeconds / LastPlayedUtc` | | Character-select row without a full load |

---

## 4. Write cadence, crash safety, and mid-rift survival

### 4.1 Atomic write

Never overwrite a live file.

```
1. Serialize payload to memory, compute PayloadHash, fill header.
2. Write to <name>.tmp.
3. Flush + close.
4. Rotate: .bak2 -> .bak3, .bak1 -> .bak2, .sav -> .bak1  (renames only)
5. Rename .tmp -> .sav   (atomic on Win32 and APFS via MoveFile/replace)
6. Delete .bak4 if present.
```

A crash at any step leaves at least one intact file. Step 4 is renames only, so
it is fast and cannot half-write.

### 4.2 Load order

```
Try .sav      -> magic ok? version supported? hash matches? -> accept
Try .bak1     -> same checks
Try .bak2, .bak3
All fail      -> DO NOT create a fresh character over the top.
                 Surface a hard error, keep the files, offer "start new
                 character" as an explicit user action only.
```

Silently generating a blank save over a corrupt one is the single worst failure
mode a looter can have. Never do it.

### 4.3 Autosave cadence

| Trigger | Scope | Reason |
|---|---|---|
| Level up | Character | Cheap, high emotional value |
| Item equipped / unequipped | Character | Debounced 5s |
| Forge transaction (craft, respec, exalt/corrupt) | Character + Account | Consumes materials; must never be lost or duplicated |
| Stash transfer | Character + Account | **Must be a single transaction** — see 4.5 |
| Class lock | Character | Irreversible |
| Rift entered | Character (+ open journal) | Establishes the rollback point |
| Rift cleared / boss killed | Character | Commits the run |
| Anchor entered | Character + Account | Natural safe point |
| Periodic | Character | Every 120s **while in the Anchor only** |
| Application quit / EndPlay | All | Sets `bCleanClose = true` |

Deliberately **not** on a timer during a rift. A mid-combat serialization hitch
in a movement shooter is a worse defect than the loss it prevents, and the
journal in 4.4 covers the gap at a fraction of the cost.

### 4.4 Mid-rift crash — the run journal

A rift run is a transaction. What must survive a crash mid-rift:

| Must survive | Mechanism |
|---|---|
| XP earned, level-ups | Journal append |
| Items dropped and picked up | Journal append (full `FBreakerItemInstance`, not a seed — the roll already happened and re-rolling it would change the item) |
| Materials/currency earned | Journal append, **applied to account only on commit** |
| Rift progress (rooms cleared, boss state) | Journal checkpoint record |
| Points spent (cannot happen mid-rift — Forge-gated) | N/A, by design |

`BreakerRun_<Guid>.sav` is an **append-only journal**, not a snapshot. Records
are small (a few hundred bytes), appended on discrete events (kill reward, loot
pickup, checkpoint), never per-frame. On next launch:

- Journal present + character file `bCleanClose == false` → offer **Recover
  Run**. Replays the journal onto the character record.
- Journal present + `bCleanClose == true` → stale; delete.
- Clean rift exit → journal folded into the character save, then deleted.

Recovery restores *rewards and rift progress*, not exact world position. The
player resumes at the rift entrance or last checkpoint with everything they
earned. This is the right trade: re-walking a corridor is annoying, losing a T0
drop is unacceptable.

### 4.5 Two-file transactions

Stash transfers and Forge crafting touch both the account and character files.
Naive ordering duplicates or destroys items.

Rule: **the account file is the ledger of record for any item in transit.**

```
Move item X from character backpack -> stash:
  1. Write ACCOUNT with X present and X.InTransitFrom = CharacterId.
  2. Write CHARACTER with X removed.
  3. Write ACCOUNT clearing InTransitFrom.

Crash after 1: item exists in both files. On load, InTransitFrom is set and
the character still holds X -> the account copy is discarded. No duplication.
Crash after 2: item exists only in the account, InTransitFrom set, character
does not hold it -> clear the flag, keep it. No loss.
```

The invariant: a crash can never produce two live copies, and can never produce
zero. Item duplication is the one bug that destroys a loot game's economy, and
it must be designed against before there is an economy to destroy.

---

## 5. Versioning and migration

### 5.1 Three version numbers, three jobs

The structs already carry `SaveVersion`, but nothing reads them. Give each one a
defined job rather than adding a fourth:

| Version | Lives on | Governs |
|---|---|---|
| `FormatVersion` (header) | File | File layout, container set, header fields. Bumps rarely. |
| `FBreakerProgressionState::SaveVersion` | Struct | Point schedule, node-id semantics, class enum |
| `FBreakerItemInstance::SaveVersion` | Struct | Affix ids, tier meanings, rarity enum, slot enum |

Per-struct versions are what let item migration proceed independently of
progression migration — and given that affix content is explicitly placeholder
until the TTK pass (`3.0`), items will version far more often than progression.

### 5.2 Migration contract

- Every version bump ships a migration step. `MigrateItem(N -> N+1)`,
  `MigrateProgression(N -> N+1)`, chained. Never a switch on "old vs new".
- Migrations are **pure functions on the deserialized struct**, unit-testable
  without an engine world. Add them to the existing automation suite; a
  migration with no test is a data-loss bug waiting for a release.
- Migration runs on load, in memory. The migrated file is written back on the
  next normal save, not eagerly — a crash during a migration write on first
  launch after a patch is the worst possible moment.
- **Unknown IDs are quarantined, never dropped.** An affix id or node id the
  current build does not recognize is preserved verbatim in an
  `UnknownPayload` array and re-emitted on save. A player who launches a beta
  branch and comes back must not lose their items.
- **Forward version = refuse to load.** A file whose `FormatVersion` exceeds the
  build's is not opened, not repaired, not overwritten. Show "this character was
  saved by a newer version."

### 5.3 Named migration cases to expect

| Change | Handling |
|---|---|
| Affix re-tuned (T1 value changes) | No migration. Stored `Value` is the roll; re-anchoring the curve does not retroactively change existing items unless we *choose* to, which is a balance decision, not a save decision. |
| Affix removed | Quarantine the rolled affix, mark item "legacy", grant a free Forge reroll of that slot. Do not silently delete. |
| Affix renamed | ID remap table in the migration step. |
| Node removed / tree restructured | Refund the points. Progression stores IDs and ranks precisely so this is a refund rather than a rebuild (`7.9`). |
| Level cap or point schedule changed | Cap is Data-Asset-driven; recompute granted points from the curve and reconcile spent-vs-granted. Excess spend → refund all points, force a free respec. Cap is 50 and locked, so this is mainly for the slice's `SLICE CAP: 10` override. |
| New equipment slot added | Default empty. Slot enum must be append-only. |
| Class enum changed | Class enum is append-only, permanently. A reorder silently converts every Tank into a Support. |

### 5.4 Enum discipline (EXTENDS the master sheet)

`EBreakerClassId`, `EBreakerEquipSlot`, `EBreakerItemRarity`,
`EBreakerWeaponArchetype`, and `EBreakerAffixCategory` are all serialized by
value today. Rule: **append-only, never reorder, never reuse a retired value.**
Affix ids are already `FName`, which is correct — prefer `FName` over enum for
anything content-authored.

---

## 6. Load-time validation

Loading is not restoring. Everything below is checked, and a failure clamps or
quarantines rather than rejecting the character:

| Check | Action on failure |
|---|---|
| Level within [1, cap-from-DataAsset] | Clamp; log |
| Spent points ≤ granted points | Refund all, force free respec |
| Node ids resolve in the current tree asset | Quarantine + refund |
| Node ranks ≤ node max rank | Clamp + refund difference |
| Ability loadout ids belong to the permanent class | Clear invalid entries |
| Item slot matches its definition's slot | Unequip to backpack |
| ≤ 4 prefixes and ≤ 4 suffixes, ≤ 7 affixes per item | Quarantine the excess |
| **≤ 3 Aberrant equipped, ≤ 1 Anomalous equipped** | Unequip the excess to backpack |
| Affix tier within [-1, 8] and value within the tier band | Clamp to band |
| Rarity tier-cap respected (Standard ≤ T3, Uncommon ≤ T1) | Clamp |
| Item count ≤ container capacity | Overflow to a locked recovery tab |

The Aberrant/Anomalous check matters most: those equip limits *are* the endgame
decision (`9.2`). If the save layer does not enforce them, they are advisory.

---

## 7. The server-authoritative target

Everything above is designed so that the transition is a change of *transport*,
not a change of *shape*.

### 7.1 What already survives the move

- Stable-id-and-number-only payloads (no pointers, no computed totals). Already
  the rule; already followed.
- The three-tier account/character/session split maps directly onto a schema:
  `accounts`, `characters`, `character_items`, `account_items` (stash),
  `currencies`.
- Guid item identity (`FBreakerItemInstance::ItemId`) is already the primary key
  a server needs for trade, mail, and audit.
- Two-phase item transfer (§4.5) is the same protocol a server uses; it just
  becomes a real DB transaction instead of a file-ordering trick.

### 7.2 What changes

| Concern | Local | Server-authoritative |
|---|---|---|
| Authority | `HasAuthority()` on the listen host | Dedicated server; client never writes |
| Item generation | Client-side roll in `BreakerLootLibrary` | Server-only. Client receives the result. Never send the seed. |
| Save trigger | Game events call the subsystem | Same events, but they enqueue mutations to a backend |
| Stash | File | Row set with a per-account lock; only one session may hold the lock |
| Settings | Local ini | Stays local (plus optional cloud mirror) — settings are the one tier that should *not* become authoritative |
| Migration | On load, in the client | Offline batch job over the DB, plus a lazy on-read path for cold accounts |
| Backups | Rotating `.bak` files | Point-in-time DB restore; the `.bak` scheme retires |
| Crash journal | `BreakerRun_*.sav` | Server-side session state; the client crashing loses nothing |

### 7.3 Design consequences to accept now

1. **`UBreakerSaveSubsystem` must expose an async, callback-based API from day
   one**, even though local file IO returns immediately. A synchronous API is
   the thing that makes the server port a rewrite. `RequestLoadCharacter(Guid,
   FOnCharacterLoaded)` — not `GetCharacter(Guid)`.
2. **A storage backend interface** (`IBreakerSaveBackend`) with a local-file
   implementation now and an HTTP/DB implementation later. The subsystem talks
   to the interface only.
3. **One writer per character at a time.** Enforce a local lock file now so the
   "two clients, one character" bug is designed out before it can ship.
4. **Never trust the client's item payload.** Even in the local build, write
   validation (§6) as if the source were hostile — that code is exactly the
   server's validation code, written early and for free.
5. The Anchor is non-instanced and shared (`8.3`, `10.1`), so account state is
   the first thing that needs a server. Everything else can lag behind.

CONFLICT (recorded, not resolved): the master sheet's Party Play tab leaves loot
distribution open (instanced / shared / need-greed). Instanced-per-player is the
only option that keeps §4.5's transaction model simple, since shared loot
introduces cross-account item transit. Recommend instanced; flagging that this
save design assumes it.

---

## 8. Migration path from today

Each step leaves the project playable, per the build-vertically rule.

**Step 1 — Subsystem extraction (no format change).**
Create `UBreakerSaveSubsystem`. Move save/load out of `ABreakerCharacter`.
Keep writing `BreakerSave0`. Acceptance: gym still saves and resumes; no `.sav`
format change; existing saves load.

**Step 2 — Header, hashing, atomic write, rotating backups.**
Introduce the header block and the `.tmp` → rotate → rename write. Read legacy
headerless `BreakerSave0` as `FormatVersion 0` and migrate it forward on first
load. Acceptance: kill the process mid-write 20 times; every launch loads a
valid save.

**Step 3 — Split account from character.**
`BreakerAccount.sav` + `BreakerChar_<Guid>.sav`. Legacy `BreakerSave0` migrates
into one character record with an empty account record. Settings move out of
the ad-hoc `RiorsEdge.Playtest` GConfig section into the account file (or a
proper `UGameUserSettings` subclass — either is fine, but pick one).

**Step 4 — Character select and multi-slot.**
A character-select screen ahead of the title's PLAY. Create / delete (with typed
confirmation and a 24h soft-delete window) / choose class at creation, which is
where `ChoosePermanentClassById` should live rather than in an in-game menu.
Acceptance: create one of each class, all five persist independently.

**Step 5 — Stash and currencies.**
Account containers plus the two-phase transfer. Anchor-gated.
Acceptance: a scripted crash injected between phases never duplicates or
destroys an item, across all three injection points.

**Step 6 — Migration framework and validation.**
Chained per-struct migrators, quarantine arrays, the §6 validation table, and
automation tests for every migrator and every validation rule.

**Step 7 — Run journal.**
Append-only journal, `bCleanClose`, and the Recover Run flow. Do this after
rifts are real content; the gym does not need it.

**Step 8 — Backend interface.**
`IBreakerSaveBackend` + async API, still local-file-backed. This is the last
local-only step and the first server step simultaneously.

---

## 9. Acceptance criteria

1. Twelve characters can exist simultaneously; each has exactly one permanent
   class; no operation on one can modify another.
2. No code path outside `UBreakerSaveSubsystem` calls `SaveGameToSlot` or
   `LoadGameFromSlot`.
3. Killing the process at any point during a save leaves a loadable file. Test
   by injecting failure at each of the five write steps, 100 iterations each.
4. A corrupt `.sav` falls through to `.bak1`/`.bak2`/`.bak3` and never results in
   a silently-created empty character.
5. A file from a newer `FormatVersion` is refused, not overwritten.
6. Every `SaveVersion` bump has a migrator and at least one automation test that
   loads a fixture of the previous version and asserts the result.
7. An unknown affix id or node id round-trips through a save/load cycle byte-
   identical.
8. A hand-edited save claiming level 90, 12 Anomalous items, and 400 spent
   points loads to a legal state: clamped level, excess unequipped, points
   refunded.
9. A crash mid-rift, after loot pickup and before rift completion, recovers the
   loot and the XP on the next launch.
10. No stash transfer sequence, interrupted at any point, produces zero or two
    copies of an item.
11. Character save payload for a fully-geared level 50 character with a full
    backpack is under 512 KB; account save with a full stash under 4 MB.
12. Autosave never causes a frame-time spike above 2 ms on the game thread
    during a rift (serialization off the game thread where the payload allows).
13. Settings persist independently of every character and survive deleting all
    characters.

---

## 10. OPEN QUESTIONS

1. **Stash scope.** Account-wide stash is recommended above, but it makes alts
   instantly geared, which softens the "gear is the entire endgame" chase for
   every character after the first. Is that acceptable, or does the stash need a
   class-restricted partition? This is a product decision, not a technical one,
   and it should be answered before Step 5.
2. **Are fragments (`1.7`) account-wide or per-character?** They unlock real
   capabilities, so account-wide means alts skip a campaign gate; per-character
   means replaying the collectible hunt five times. The master sheet does not
   say. EXTENDS.
3. **Is there hardcore / permadeath?** It changes the entire backup story — a
   rotating-backup scheme is a cheat vector if a character can die permanently.
   Decide before Step 2, not after.
4. Does the account file need a stash-lock for a second concurrent local
   session, or is single-instance enforcement sufficient until the server exists?
5. What is the retention policy for a deleted character — the 24h soft delete
   proposed above, or immediate and irreversible?
6. Do rift runs need mid-run *position* recovery, or is entrance/checkpoint
   recovery sufficient? Entrance recovery is assumed above and is much cheaper.
7. Are crafting materials a separate currency or item-derived? (Open in `4.9`.)
   Item-derived materials live in the stash and inherit its transaction cost;
   a scalar currency does not.
8. Does the Anchor-only stash rule survive contact with the endgame loop? If
   Anomaly runs are long, players will want a mid-run stash and that reintroduces
   cross-transaction risk.
9. Should item-generation seeds be stored alongside rolled results for audit and
   dupe detection? Cheap now, impossible to backfill later.
10. Cloud save for the pre-server period — Steam Cloud, or none? Steam Cloud plus
    rotating local backups has a known conflict-resolution failure mode that
    `WriteCounter` mitigates but does not solve.
