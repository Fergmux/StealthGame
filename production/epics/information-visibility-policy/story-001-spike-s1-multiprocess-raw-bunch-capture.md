# Story 001: Spike S1, multi-process raw-bunch capture shows no identity pointers to non-owners

> **Epic**: Information Visibility Policy
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

The 2026-10-25 spike report (`production/qa/spikes/2026-10-25_S1-S2-S14.md`) proved at property level, in one PIE process, that the three identity pointers (`APawn::PlayerState`, `APawn::Controller`, `AActor::Owner`) never reach a non-owner connection once they are `COND_OwnerOnly` (0 sends versus 97 in the baseline). It was explicitly not byte-exact and not multi-process or multi-machine. This spike closes that gap: separate processes on a real UDP path under the leak net profile, a capture that records the bunches a non-owning client actually receives, and the choice of the capture mechanism that ADR-0008 section 6 leaves to S1. Every later parity story (AC-IVP-02, AC-IVP-37, AC-IVP-45, the transport classifier) trusts that mechanism, so it must first pass its own negative control. Rejoin, seamless-travel and PawnRecovery coverage of S1 are carried by story-002 and story-003.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-003` (UE default overrides of R5: pawn links, Instigator, possession ForceNetUpdate, RemoteViewPitch16, spectator); also `TR-ivp-007` (leak catalogue with CRITICAL ship gate; per-commit canaries)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules; also ADR-0008: Test and automation strategy for leak and parity tests (section 6, capture mechanism)
**ADR Decision Summary**: ADR-0001 section 4.1 resets `PlayerState` (by name), `Controller` and `Owner` to `COND_OwnerOnly` on the pawn class so non-owners receive none of them for either pawn kind; the test is absence from every non-owner changelist (S1, AC-IVP-01/02/37), not "equal null". ADR-0008 section 6 requires a client-side and a server-side send capture and lets S1 choose the mechanism (NetTrace analysis or a harness-only `UNetConnection`/replicator receive hook), recorded as an ADR-0008 amendment.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Networking is HIGH risk for 5.4-5.8. Legacy `IpNetDriver` (Iris pinned off, `net.Iris.UseIrisReplication=0` in `Config/DefaultEngine.ini`). `APawn::PlayerState` is private, so it is reset by name; a misspelling is a fatal startup error. `RemoteViewPitch16` (5.6) replaced the 8-bit `RemoteViewPitch` (E8). The legacy driver uses shared serialization (`WriteSharedProperty`); the per-connection `SendProperties_r` lines still list parents. This spike owns the [U] claim "S1" in ADR-0001.

**Control Manifest Rules (this layer)**:
- Required: **`AStealthPawn` resets `PlayerState` (by name), `Controller` and `Owner` to `COND_OwnerOnly`** with the condition-only `ResetReplicatedLifetimeProperty` overload — source: ADR-0001
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- Forbidden: **Never prove multi-process criteria in in-process PIE only; never select tiers with `Automation RunFilter`** — source: ADR-0008
- Guardrail: **Tests**: per-commit pipeline ≤ 10 min; nightly cook + T3 estimated 1-2 h [U] — source: ADR-0008

---

## Acceptance Criteria

*From the story's spike definition (spike story; criterion written for the S1 condition left open in ADR-0001's Amendments):*

- [x] GIVEN a Development build with one listen-server process and at least three separate client processes on one machine (two machines if available) under net profile StealthLeak2 (PktLag 100 ms, 2 % loss), and 2 player and 2 NPC pawns in view of a third client, WHEN the non-owning client's received bunches (and the server's per-connection send changelists) are captured across spawn, takeover and AI hand-over, THEN PlayerState, Controller and Owner appear in no non-owner changelist for either pawn kind; the negative control (APawn::PlayerState at COND_None) is detected by the same capture; the winning capture mechanism (NetTrace analysis or a harness-only UNetConnection/replicator receive hook) is recorded as an ADR-0008 amendment; and the verdict (byte-level where the mechanism allows, with the two non-pointer differentiators of the 2026-10-25 report, RemoteViewPitch16 and CharacterInputState, re-measured) is written to production/qa/spikes/.

---

## Implementation Notes

*Derived from ADR-0001 section 4.1 and Validation Criteria (S1), ADR-0008 section 6:*

- This is a throwaway spike. It may run on the prototype pawn using the `stealth.Spike.IdentityTrims` pattern (reference only: `Source/StealthGame/Private/Spikes/StealthIdentitySpike.{h,cpp}`, analyzers in `production/qa/spikes/2026-10-25_tools/`). Production stories re-prove the result on production classes (AC-IVP-02, AC-IVP-37).
- Candidate mechanisms: (a) `NetTrace` output analysed offline; (b) a harness-only receive hook on `UNetConnection`/the replicator. The spike must try the one it prefers, run its own negative control first (a pawn variant with `APawn::PlayerState` at `COND_None` must be detected), and record the winner and the reason as an ADR-0008 amendment. Whatever wins records per actor: class, NetGUID class (null / mapped / unmappable), property handles and order, bit-length class, condition, components, subobjects and RPC set (ADR-0008 section 6).
- Re-measure the two non-pointer differentiators found on 2026-10-25: `RemoteViewPitch16` (reached non-owners only for player-controlled pawns; ADR-0003 section 3 fixes it by writing it from `HeadLook` for every pawn) and the sample `CharacterInputState` property (`COND_SkipOwner`, set only by a player's Server RPC). Record whether they still differ; they are recorded, not gated, here.
- Net profile: `[PacketSimulationProfile.StealthLeak2]` (PktLag 100 ms, PktLoss 2 %) in `Config/DefaultEngine.ini` if absent; record the measured RTT in the profile comment (ADR-0008 section 5). Packet loss is not seedable, so assert bounds.
- Record run length, pawn lifetimes and the number of clients in the report; the criterion fixes none.
- Rejoin, seamless travel and PawnRecovery are out of scope (story-002, story-003).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-002: S2 PlayerState trims across seamless travel and rejoin
- story-003: S14 and S1 rejoin and PawnRecovery possession
- story-044: the production raw-bunch schema parity gate (AC-IVP-02)
- story-019: the per-commit possession canary (AC-IVP-37)

---

## Files

- `production/qa/spikes/2026-10-25_S1-multiprocess.md` (the verdict)
- `Source/StealthTestHarness/Private/Capture/StealthNetCapture.{h,cpp}` (the chosen capture mechanism: harness-only `UStealthCaptureActorChannel` receive/send hook)
- `Source/StealthTestHarness/Private/Scenarios/StealthHarnessScenarioS1.cpp` (T3 scenario `S1`); runner `StealthHarnessRun.{h,cpp}` from story tdc-017 (adds the 0.1 s pawn truth and the C<n> naming used here)
- `Source/StealthGame/Private/Spikes/StealthIdentitySpike.{h,cpp}` (spike bit 16: the negative control, pawn PlayerState back to `COND_None`)
- `tools/ci/mp/analyze_s1.py`
- `docs/architecture/adr-0008-test-automation-leak-parity.md` (amendment: the winning mechanism), `docs/architecture/adr-0001-crowd-identity-rules.md` (amendment: S1 multi-process reported)
- `Config/DefaultEngine.ini` profile `StealthLeak2` (story tdc-017)
- Evidence: `production/qa/evidence/story-001-spike-s1-multiprocess-raw-bunch-capture/`

---

## QA Test Cases

- **AC-1**: S1 multi-process raw-bunch capture
  - Given: Development build, one listen server and at least 3 client processes (2 machines if available), profile StealthLeak2; 2 player and 2 NPC pawns in view of a third client.
  - When: the non-owner's received bunches and the server's per-connection send changelists are captured across spawn, takeover and AI hand-over.
  - Then: PlayerState, Controller and Owner appear in 0 non-owner changelists for both pawn kinds; the negative control (APawn::PlayerState at COND_None) is detected by the same capture; the winning mechanism is recorded as an ADR-0008 amendment; the verdict is written to production/qa/spikes/ with RemoteViewPitch16 and CharacterInputState re-measured.
  - Edge cases: the criterion gives no run length or lifetime count, so record them in the report; the re-measure has no pass threshold, it is recorded only. (Tier: T3 multi-process, deferred to a cooked Development build.)

---

## Test Evidence

*Governed by `qa.level`: tests are advisory only at `qa.level: minimal`; this project runs `qa.level: full`.*

**Story Type**: Integration
**Required evidence**:
- Integration: a written spike report `production/qa/spikes/<date>_S1-multiprocess.md` citing the capture artefacts and the negative-control run (a documented measurement, not a unit test).

**Status**: [x] Spike report `production/qa/spikes/2026-10-25_S1-multiprocess.md` (PASS) with capture artefacts and the negative-control run

---

## Dependencies

- Depends on: None (may run on the prototype; harness integration follows once tuning-data-config Exit A delivers the module skeleton and harness)
- Unlocks: `production/epics/information-visibility-policy/story-002-spike-s2-seamless-travel-and-rejoin-playerstate-trims.md`, `production/epics/information-visibility-policy/story-003-spike-s14-s1-rejoin-and-pawnrecovery-possession.md`, `production/epics/information-visibility-policy/story-008-spike-fixed-width-netserialize-byte-capture.md`, `production/epics/information-visibility-policy/story-018-engine-default-overrides.md`, `production/epics/information-visibility-policy/story-043-transport-classifier-auc.md`, `production/epics/information-visibility-policy/story-044-raw-bunch-schema-parity.md`, `production/epics/information-visibility-policy/story-055-clock-exchange-owner-only-audit.md`

---

## Completion Notes
**Completed**: 2026-10-25
**Verdict**: PASS — 0 identity-pointer sends to non-owner connections and 0 steady-state non-owner receipts, player and NPC pawns, across spawn, takeover, AI hand-over and kill; negative control detected (35 non-owner PlayerState sends; every client received it); mechanism recorded (ADR-0008 amendment); RemoteViewPitch16 and CharacterInputState re-measured (still player-only, recorded).
**Criteria**: 1/1 (AC-1; byte level = bunch payload bits, see the report's CONCERNS on per-property bits and pointer NetGUID class)
**Decisions**: capture = harness-only `UActorChannel` hook bracketing the engine's replication log points (not NetTrace: no headless per-bunch property export); profile applied on every process (symmetric); 12 NPCs (`-NpcCount 12`) to keep the host at ~200 fps under VeryVerbose logging; one machine (no second machine available); uncooked Development editor binary in `-game` mode instead of a cooked build (Gauntlet/cook are tdc-020/023).
**Deviations**: none against the AC beyond the recorded limits (one machine; uncooked).
**Test Evidence**: report + evidence folder (analysis JSON and gzipped records of runs s1-trims-b and s1-negctl-b).
**Run result**: N/A — headless multi-process measurement (`-nullrhi`); nothing player-visible changes
**Reviewed by**: focused self-review (unattended run): capture only installs with `-StealthNetCapture`, collects on the game thread only, removes its output device on pre-exit; harness excluded from Shipping/Test.
**Unblocks (S1 side)**: story-018 and story-044 now wait only on their other gates (S13/S14 and S10 respectively); story-002/003/043/055 are blocked on other epics or spikes (unchanged).
