# Story 039: No Server/Client/Multicast RPC parameter contains a participant id or identity key

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules
**ADR Decision Summary**: ADR-0001 keeps the pawn-to-participant link and every identity fact off the wire except through owner-only carriers: team and owner data travel only in the C-TeamView components on the receiving controller, `ServerOnly` stores such as the Roster are never replicated properties, and client messages never name a participant (the server derives the participant from the receiving connection's controller; Roster R2). A reflection audit that fails the build is the stated enforcement for new replicated surface.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0001 post-cutoff items: `APawn::RemoteViewPitch16` (5.6) and the `net.Iris.UseIrisReplication` default of 0; neither is used here. Verification items are spikes S1, S2, S14 (S2 and S14 PASS; S1 PASS for the three pointers only). The scan uses long-standing reflection (`UFunction`, `FProperty`, `FUNC_Net`) and needs a re-check of property class names (for example `FOptionalProperty`) against the 5.8.4 headers before writing.

**Control Manifest Rules (this layer)**:
- Required: Every Session RPC is owner-only; the participant is derived from the receiving connection — source: ADR-0005
- Required: Every leak, parity, statistical and lint gate includes a negative control the suite detects — source: ADR-0008
- Required: Every new replicated field, fact, audience, condition or replicated class ships with a Visibility Policy row and a signed-off allowlist row; the reflection audit fails the build otherwise — source: ADR-0001, ADR-0003
- Forbidden: Never use a multicast Session RPC — source: ADR-0005
- Forbidden: Never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail — source: ADR-0008
- Guardrail: Tests: per-commit pipeline at most 10 min (the scan is a headless reflection pass) — source: ADR-0008

This story delivers a reflection scan that fails if any Server, Client or Multicast RPC parameter in the project's modules is, or contains, a `FParticipantId` or `FIdentityKey`. It exists because an RPC carrying a participant id would let a client name another participant and would put identity on the wire, which the Pillar-1 leak model forbids. It realises GDD R2 ("no RPC parameter ever carries a ParticipantId or an identity key"), and relies on story 001 declaring both types as distinct reflected struct types so the scan is meaningful.

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-04** [Int] `StealthGame.Roster.NoRpcCarriesParticipantId`: GIVEN every Server/Client/Multicast UFUNCTION in the game module, WHEN parameter types are scanned by reflection, THEN none is or contains, transitively through structs and containers, a `ParticipantId` or `IdentityKey` type. Both are distinct struct types, not raw integers or strings, which makes the scan meaningful (R2).

---

## Implementation Notes

*Derived from ADR-0001 Implementation Guidelines, ADR-0005 guideline 6, ADR-0008 and GDD R2:*

- **What is scanned.** Every `UFunction` with `FUNC_Net` (this covers the Server, Client and NetMulticast RPC kinds) in the scanned packages. Iterate loaded classes (for example `TObjectIterator<UClass>`) and their declared functions, filtering by owning package. For each parameter (`CPF_Parm`) walk the property tree: `FStructProperty` into the struct's own properties, `FArrayProperty` inner, `FSetProperty` element, `FMapProperty` key and value, `FOptionalProperty` inner, recursing with a visited-struct set to stop cycles. Compare the `UScriptStruct` against `FParticipantId` and `FIdentityKey` (story 001's distinct types). Do not follow `FObjectProperty` targets (objects are replicated by reference, not by value).
- **Prerequisite on story 001.** `FParticipantId` and `FIdentityKey` must be `USTRUCT`s so that parameters of those types appear as `FStructProperty` the scan can see. A raw integer or string would make the scan vacuous; if story 001 did not make them reflected, fix it there, not by weakening the scan.
- **Module scope.** The AC says the game module; the qa-lead asks that the scope be stated. Scan the game module package (`/Script/StealthGame`) and the Foundation package (`/Script/StealthFoundation`) at minimum, and state both in the test. Foundation code cannot include game-module headers, so the test finds classes by reflection only and never `#include`s them.
- **Positive control (ADR-0008).** Add a synthetic, test-only `UCLASS` with a `UFUNCTION(Server)` whose parameter is, or contains (struct, array of struct), a `FParticipantId`, and assert the scan reports it, so the scan is shown to detect a violation. Compile it only under `WITH_STEALTH_TEST_SEAMS` (never Shipping), and make the production scan skip the control class by an explicit name constant while the control run scans it by name.
- **Result reporting.** On a violation, fail the test with the class, function and parameter path. The scan is an observation helper; the pass/fail decision lives in the test.
- **Open point:** ADR-0007 defines four runtime modules plus the editor and harness modules. The AC and QA scope the scan to the game module and Foundation. Whether `StealthGameplay` and `StealthPresentation` should be included now (the reflection pass is cheap and they will own RPCs such as pawn commands and pings) is not stated; recommend including them and ask the lead-programmer to confirm.
- **Prototype disposition:** no prototype counterpart (a new guard). The prototype's RPCs predate the participant-id types and carry none.
- **Header naming (ADR-0007).** The scan helper lives under `Private/Tests/Roster/`, so no new public header; if one is needed it must be `Stealth`-prefixed and project-unique.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 001 (Participant record fields equal the R3 whitelist exactly): owns the declaration of the distinct `FParticipantId` and `FIdentityKey` struct types.
- Story 004 (Every roster call on a client is rejected or answers not-targetable and faults): owns the client-side rejection of roster calls.
- Story 040 (PlayerState replicates only the allowlist and engine defaults are disabled or constant): owns the PlayerState replicated-property audit.
- Story 046 (Non-owned pawns carry null PlayerState, Controller and Owner for players and NPCs alike): owns the raw-bunch PIE proof.
- Session & Travel stories (outside this epic): own the individual Session RPCs; this scan only checks their parameter types.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterNoRpcParticipantIdTest.cpp` (class `FRosterNoRpcCarriesParticipantIdTest`, test name `StealthGame.Roster.NoRpcCarriesParticipantId`)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterRpcScan.h` (scan helper and the test-only positive-control class, `WITH_STEALTH_TEST_SEAMS`)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterRpcScan.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Automated test specs (Integration):**

- **AC-1**: no RPC parameter contains a participant id or identity key.
  - Given: every UFunction with a net flag (FUNC_Net: Server, Client, Multicast) in the game module and the Foundation module.
  - When: parameter types scanned by reflection, transitively through structs and containers.
  - Then: none is or contains FParticipantId or FIdentityKey.
  - Edge cases: add a synthetic positive control with an offending parameter to show the scan is not vacuous. State the module scope.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterNoRpcParticipantIdTest.cpp` — automated test must exist and pass (OR a documented playtest; `qa.level` is full, so the automated test is required).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-001-record-schema-whitelist.md`
- Unlocks: None
