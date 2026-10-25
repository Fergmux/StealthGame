# Story 044: Raw-bunch schema parity holds, structural leak count S = 0

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: spike story-005 (S10) has not reported; needs AStealthPawn. S1 reported PASS 2026-10-25 with the capture mechanism chosen (production/qa/spikes/2026-10-25_S1-multiprocess.md; ADR-0008 amendment) and S9 reported PASS (production/qa/spikes/2026-10-25_S9.md)
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

A non-owning client must not be able to tell player pawns from NPC pawns by what the server sends: property handles and order, bit-length class, conditions, NetGUID class, components, subobjects and the RPC set must be structurally identical across spawn, cull and re-open, death, hand-over and rejoin. This story is the production gate for that claim (F1, structural leak count `S = 0`), run on real `AStealthPawn` instances in a multi-process cooked Development run. It is the nightly proof behind leak rows L1, L4 and L31 and feeds the CRITICAL ship gate (story-049).

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-007` (Leak catalogue with CRITICAL ship gate; per-commit canaries)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (sections 1 T3, 3, 5, 6); ADR-0003: Replication, relevancy and the concealment cull hybrid (section 8, Validation Criteria); ADR-0001: Crowd identity rules (section 4.1, section 5)
**ADR Decision Summary**: ADR-0008 section 6 gives the harness a client-side capture (what a non-owning client receives) and a server-side send capture (what the server sends to one connection), recording per actor the class, NetGUID class, property handles and order, bit-length class, condition, components, subobjects and RPC set; spike S1 chooses the mechanism, and whatever wins must first pass its own negative control (`COND_None` `PlayerState` detected) before any parity result is trusted.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: ADR-0008 Verification Required (2): a client-side capture of per-pawn property handles, order, bit-length class, NetGUID class and RPC set under the legacy NetDriver is [U]; S1 picks between net trace (legacy property scopes exist at `ENetTraceVerbosity::Trace` [V `RepLayout.cpp:1929, 2799, 3130`; `DataReplication.cpp:1098`], full verbosity needed [U]) and a harness receive hook. ADR-0003 E10 (closed channel stays closed) [V], in-flight cull/un-cull race [U, S10]; E11 (re-open creates a new client actor, NetGUID reuse [U, S10]); E12 (`WithNetSerializer` structs sent whole with exactly the bits written) [U, fixed-width byte capture, story-008].

**Control Manifest Rules (this layer)**:
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- Required: **`AStealthPawn` resets `PlayerState` (by name), `Controller` and `Owner` to `COND_OwnerOnly`** with the condition-only `ResetReplicatedLifetimeProperty` overload — source: ADR-0001
- Forbidden: **Never prove multi-process criteria in in-process PIE only; never select tiers with `Automation RunFilter`** — source: ADR-0008
- Guardrail: **Tests**: per-commit pipeline <= 10 min; nightly cook + T3 estimated 1-2 h [U] — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, AC-IVP-02, scoped to this story:*

- [ ] **AC-IVP-02** [PIE-3][Net][S1][S10] `StealthGame.Visibility.RawBunchSchemaParity`: GIVEN ≥ 3 clients and ≥ 100 pawn lifetimes including spawn, cull and re-open, death, hand-over and rejoin, WHEN a non-owning client records each pawn's property handles and order, bit-length class, condition, NetGUID class, components, subobjects and RPC set, THEN `S = 0` (F1). Negative control: `COND_None` `APawn::PlayerState` gives `S ≥ 1`.

---

## Implementation Notes

*Derived from ADR-0008 sections 1, 3, 5, 6 and ADR-0003 section 8 / Validation Criteria:*

- T3 test `StealthGame.Visibility.MP.RawBunchSchemaParity`: a harness scenario under `Source/StealthTestHarness/Private/Scenarios/` driven by a Gauntlet node in `Build/Scripts/`, on a cooked Development listen host plus at least three client processes under `StealthLeak1` or `StealthLeak2` (`-PktEmulationProfile=<Name>`, packet loss is not seedable so assert bounds). The run asserts the legacy replication system and records it in the result JSON (ADR-0008 section 5). Travel and rejoin cannot be proven in single-process PIE (ADR-0008, amendment 2026-10-25b).
- Use the capture mechanism chosen by story-001 (recorded there as an ADR-0008 amendment). Before any parity result is trusted the capture must detect its own negative control: a pawn variant with `APawn::PlayerState` at `COND_None` must give `S >= 1` in the same suite (ADR-0008 section 3; the control type lives only in `StealthTestHarness`, never Shipping).
- Cover at least 100 pawn lifetimes that include spawn, cull and re-open (the cull path and re-open behaviour proven by story-004 (S9) and story-005 (S10), here on `AStealthPawn`), death, hand-over (AI hand-over and takeover) and rejoin. Compare the per-pawn schema for player and NPC pawns; `S` is the F1 structural leak count and must be 0.
- The two non-pointer differentiators found in the 2026-10-25 S1 report, `RemoteViewPitch16` and `CharacterInputState`, are re-measured by story-001; this story asserts them inside the schema comparison on production classes.
- Serialiser byte-identity of `FActionState` and `FDeathState` (ADR-0003 section 4, E12) is recorded by story-008; if it reports E12 refuted, the bit-length class comparison here must be re-read with the fixed-width finding in mind (this is not an intra-epic dependency edge, so treat story-008's report as an input to read, not a prerequisite).
- Spike gates: story-001 (S1), story-004 (S9) and story-005 (S10) must have reported. Prototype: reference only `Source/StealthGame/Private/Spikes/StealthIdentitySpike.*` and the analyzers in `production/qa/spikes/2026-10-25_tools/`; nothing is promoted. Replaces: none (new); the prototype's unconditional `PlayerState`/`Controller`/`Owner` replication on `AStealthCharacter` is replaced by the pawn story in shared-character-locomotion.
- Cross-epic needs: shared-character-locomotion epic (`AStealthPawn`, `UStealthPawnFactory`), tuning-data-config Exit A (harness, Gauntlet, CI, net profiles in `Config/DefaultEngine.ini`), participant-state-roster and session-travel epics (rejoin paths, since single-process stand-ins are not enough for the rejoin lifetimes).
- Unblocks when: stories 001, 004 and 005 report, story-015 audit exists, and `AStealthPawn` is built.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-001: choosing and validating the capture mechanism (spike)
- story-004 and story-005: the S9 and S10 spikes (cull latency, re-open behaviour)
- story-015: the replicated-property reflection audit (T1, property level)
- story-019: the per-commit possession canary (single-process, three pointers)
- story-043: transport classifier AUC (cadence and bunch-size statistics)
- story-054: net-config runtime differential

---

## Files

*Planned (module folders are created by tuning-data-config Exit A and do not exist yet):*

- `Source/StealthTestHarness/Private/Scenarios/` (T3 scenario for `StealthGame.Visibility.MP.RawBunchSchemaParity`, including the `COND_None` negative-control pawn variant)
- `Build/Scripts/` (Gauntlet node)
- capture tooling under `Source/StealthTestHarness/Private/Capture/` or `tools/ci/` as decided by story-001

---

## QA Test Cases

- **AC-1**: AC-IVP-02 `StealthGame.Visibility.MP.RawBunchSchemaParity` (T3)
  - Given: at least 3 clients and at least 100 pawn lifetimes covering spawn, cull and re-open, death, hand-over and rejoin.
  - When: a non-owner records each pawn's property handles and order, bit-length class, condition, NetGUID class, components, subobjects and RPC set.
  - Then: `S = 0` (F1); negative control: `COND_None` `APawn::PlayerState` gives `S >= 1`.
  - Edge cases: the capture mechanism must pass its own negative control before any result is trusted (ADR-0008 section 6).

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: T3 test under `Source/StealthTestHarness/Private/Scenarios/` with its nightly result JSON, OR a documented playtest/spike report

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md`, `production/epics/information-visibility-policy/story-004-spike-s9-cull-latency-and-explicit-channel-close.md`, `production/epics/information-visibility-policy/story-005-spike-s10-reopen-behaviour-and-netguid.md`, `production/epics/information-visibility-policy/story-015-replicated-property-audit.md`; shared-character-locomotion epic (`AStealthPawn`); tuning-data-config Exit A (harness, Gauntlet, CI)
- Unlocks: None
