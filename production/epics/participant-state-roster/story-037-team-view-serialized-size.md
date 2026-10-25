# Story 037: A maximal C-TeamView net-serializes in at most 96 bytes

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules
**Also governed by**: ADR-0020: Objectives data and team-view sub-structs
**ADR Decision Summary**: ADR-0001 delivers team data only through the C-TeamView carrier: replicated `UActorComponent`s derived from `UTeamViewComponentBase` on each `APlayerController`, which is relevant only to its owner, so a part reaches exactly one connection. The Roster part carries the owner block `{PublicHandle, Team, State, RoundEpoch, WrongTargetTally, LastResultSeq, T_due}` and at most four teammate entries `{PublicHandle, State, PawnRef, IsConcealed, RetireAtServerTime, bReconnecting, ReconnectEndCause}`, pushed on change only and at most once per server frame. ADR-0020 adds feature team views (`FObjectiveTeamView`) as separate components on the same controller, so they sit outside this struct and its size budget.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0001 post-cutoff items: `APawn::RemoteViewPitch16` (5.6) and the `net.Iris.UseIrisReplication` default of 0 (legacy driver, Iris pinned off); neither is used by this struct. ADR-0001 verification items are spikes S1, S2, S14 (S2 and S14 PASS 2026-10-25, S1 PASS for the three pointers only). The `stat net` share check is spike S6 (not yet reported) and is advisory only. Component replication on the controller needs `bReplicateUsingRegisteredSubObjectList` set by the controller class (ADR-0020 E2).

**Control Manifest Rules (this layer)**:
- Required: Team-audience data travels only in a `UTeamViewComponentBase` subclass on the receiving teammate's own PlayerController; owner-audience data only on the owner's PlayerController — source: ADR-0001
- Required: The Roster part carries owner `{Handle, Team, State, RoundEpoch, WrongTargetTally, LastResultSeq, T_due}` (last three Owner-only) and per teammate `{Handle, State, PawnRef, bIsConcealed, RetireAtServerTime}` plus Team-audience `bReconnecting` and `ReconnectEndCause`; the Roster push step is the only writer — source: ADR-0001, ADR-0019
- Required: Outcome-bearing replicated structs serialise to a fixed bit width — source: ADR-0003
- Forbidden: Never add a teammate connection field other than the one `bReconnecting` bit (no ping, time, reason or address), and never send it to a non-teammate — source: ADR-0001, ADR-0005
- Forbidden: Never use a team-filtered actor, a per-team replica actor or a team relevancy rule — source: ADR-0001
- Guardrail: C-TeamView pushes change-only, at most 1 per frame, within the 50 KB/s per-client budget — source: ADR-0001

This story defines the replicated C-TeamView struct (the Roster part) and its custom `NetSerialize`, and proves that a worst-case update (four teammates, every field at its maximum) stays within 96 bytes. It exists so the carrier's bandwidth is bounded and fixed-width before any push step relies on it. It realises GDD R6 (C-TeamView contents) and F7 (carrier size: about 90 B at `TeamSize` 5, ceiling 96 B).

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-33** [Core] `StealthGame.Roster.TeamViewSerializedSize`: GIVEN a C-TeamView with 4 teammates and every field at its maximum value, WHEN it is net-serialized through the struct's serializer, THEN it is ≤ 96 B (F7; raised from 64 B for the fields added by user decision 2026-10-25). The ≤ 1 % budget share is checked by a manual `stat net` profile in a [MP-9][S6] 5v5 session and recorded as advisory evidence.

---

## Implementation Notes

*Derived from ADR-0001 sections 5-6 and Key Interfaces, ADR-0007 section 2, GDD R6 and F7:*

- **Types (client-safe).** Declare in `Source/StealthFoundation/Public/View/StealthRosterTeamView.h` (no `Server/` include; it is a wire struct): `FTeamViewOwnerPart {Handle, Team, State, RoundEpoch (uint32), WrongTargetTally (uint8), LastResultSeq, T_due (double, 0 = none)}`; `FTeamViewTeammateEntry {Handle, State, PawnRef (TObjectPtr<APawn>, never `AStealthPawn`: Foundation may not depend on StealthGameplay), bIsConcealed, RetireAtServerTime (double), bReconnecting, ReconnectEndCause}` where `EReconnectEndCause` is a `uint8` enum `{None=0, Rejoined=1, GraceExpired=2, Cleared=3}`; and the `FTeamView` wrapper holding the owner part plus a fixed-capacity array of at most 4 teammate entries (`TeamSize - 1`). The wrapper struct is the unit that the AC serializes.
- **Fixed-width custom serializer.** Give `FTeamView` `WithNetSerializer` via `TStructOpsTypeTraits` and a `NetSerialize(FArchive&, UPackageMap*, bool&)` that writes a count then each field at a fixed width: state 2 bits, concealed 1 bit, `bReconnecting` 1 bit and `ReconnectEndCause` 2 bits packed into one flags byte (6 of 8 bits), `PawnRef` through the package map, the two server times as 8-byte doubles. F7 estimate: owner block about 22 B, entry about 17 B, so 22 + 4 x 17 = about 90 B, or 94 B if the cause is serialized as its own `uint8`; the 96 B ceiling leaves only about 6 B (2 B unpacked) for array and struct headers, so keep headers minimal.
- **Worst-case test input.** Every field at its maximum: handle max, `RoundEpoch` max, tally 255, `LastResultSeq` max, `T_due` and `RetireAtServerTime` at large values, state and cause at their highest enum values, flags all set. Serialize with an `FNetBitWriter` and a stub `UPackageMap` that writes a worst-case-width NetGUID for `PawnRef` (a packed NetGUID can be wider than the 4 B the F7 estimate assumes; with the 94 B packed layout there is about 2 B of headroom, so make the stub's width explicit in the test). Assert `GetNumBytes() <= 96`.
- **No dependency on the push step.** This story defines the data shape and serializer only. Writing the struct (the Roster push step, coalescing once per frame, owner feed, rebind) is covered by stories 038 and 041; the component that carries it is `URosterTeamViewComponent : UTeamViewComponentBase` (ADR-0001 Key Interfaces). Do not implement the push step here.
- **Allowlist.** Every replicated field ships with a Visibility Policy row and a signed-off allowlist row (manifest rule); the C-TeamView Roster rows already exist in the IVP; add the struct to the reflection audit's allowlist when the audit exists.
- **Advisory evidence.** The `stat net` share check (<= 1 % of the per-client budget, [MP-9][S6], 5v5) waits on spike S6 and is recorded as advisory evidence in `production/qa/evidence/`; it does not block the Core assertion.
- **Open point:** ADR-0001's Key Interfaces sketch shows `URosterTeamViewComponent` with two replicated properties (`OwnerPart` and `Teammates`) while AC-PSR-33 and F7 measure one serialized view. This story introduces the `FTeamView` wrapper so the AC's "through the struct's serializer" has one subject; confirm with the network programmer that the component then replicates one `FTeamView` property, and amend the ADR sketch if so.
- **Open point:** the type of `LastResultSeq` (`FClientCommandSeq`) is owned by the command-interface design (ADR-0011) and may not exist in code yet; declare a width-equivalent placeholder in `StealthRosterTypes.h` and swap it when the real type lands, keeping the serialized width.
- **Prototype disposition:** supersedes the prototype's per-field replication of team and life state to all clients (`AStealthPlayerState::Team`, `LifeState` in `Source/StealthGame/Public/StealthPlayerState.h`); `Source/StealthGame/Public/StealthTypes.h` enums are superseded by Foundation View wire types. Nothing here is ported.
- **Header naming (ADR-0007).** Stealth prefix, project-unique names, under `Public/View/`: `StealthRosterTeamView.h`, `StealthRosterTypes.h`; implementation `Private/Roster/StealthRosterTeamView.cpp`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 038 (SetOwnerTally and SetOwnerRespawnDue feed only the owner view with the full precedence): owns the owner-view cache, the push step feed and coalescing.
- Story 041 (A replaced controller or reclaim re-pushes C-TeamView within one server frame): owns `RebindView`, `OnViewRebound` and the controller-swap re-push.
- Story 042 (PawnRef resolves in either arrival order of pawn and team-view entry): owns client-side `PawnRef` resolution.
- Story 051 (A teammate's reconnecting flag and end cause follow the event-only clear rules in all six cases): owns the flag and cause writing rules.
- Story 045 (Opponents receive nothing but allowlisted movement updates after a kill of a player, NPC or informant), story 046 (Non-owned pawns carry null PlayerState, Controller and Owner for players and NPCs alike) and story 047 (No opponent count reaches a client before the round ends): PIE verification that the carrier reaches only its owner.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Public/View/StealthRosterTeamView.h` (new: `FTeamViewOwnerPart`, `FTeamViewTeammateEntry`, `EReconnectEndCause`, `FTeamView`, `TStructOpsTypeTraits`)
- `Source/StealthFoundation/Private/Roster/StealthRosterTeamView.cpp` (new: `FTeamView::NetSerialize`)
- `Source/StealthFoundation/Public/View/StealthRosterTypes.h` (add handle, team, state types if not already present from story 001)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTeamViewSerializedSizeTest.cpp` (class `FRosterTeamViewSerializedSizeTest`, test name `StealthGame.Roster.TeamViewSerializedSize`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Automated test specs (Logic):**

- **AC-1**: a maximal C-TeamView net-serializes in at most 96 bytes.
  - Given: an FTeamView with 4 teammates and every field at its maximum value, an FNetBitWriter with a stub package map for PawnRef.
  - When: NetSerialize runs.
  - Then: written size at most 96 bytes.
  - Edge cases: the stat net share (S6, 5v5) is advisory evidence only and does not block.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTeamViewSerializedSizeTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.
- Advisory (non-blocking): `stat net` profile of a 5v5 session after spike S6, recorded in `production/qa/evidence/`.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-001-record-schema-whitelist.md`
- Unlocks: `production/epics/participant-state-roster/story-038-owner-view-feed.md`, `production/epics/participant-state-roster/story-041-team-view-repush-on-controller-swap.md`, `production/epics/participant-state-roster/story-042-team-view-pawn-ref-ordering.md`, `production/epics/participant-state-roster/story-045-pie-elimination-leak-diff.md`, `production/epics/participant-state-roster/story-046-pie-pawn-pointer-dump.md`, `production/epics/participant-state-roster/story-047-pie-opponent-counts-never-replicated.md`, `production/epics/participant-state-roster/story-048-pie-host-view-equals-client-view.md`, `production/epics/participant-state-roster/story-051-pie-teammate-reconnecting-flag.md`
