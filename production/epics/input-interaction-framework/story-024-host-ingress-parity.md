# Story 024: AC 70 HostIngressParity

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-001`, `TR-iif-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story proves that the listen-server host's local input takes the same ingress path as a remote client's, and it is the verification report for ADR-0011 item **V6** (host parity). GDD R2 states that the host's controller calls the same `IngestCommand(Source = HostLocal)` that the Server RPC body calls, with a synthesised `ClientCommandSeq`, the same ingress checks and its own mailbox, and that its replies are enqueued and flushed with everyone else's, never delivered inline. A design review on 2026-10-24 removed an earlier host bypass because it was an `NM_ListenServer` special case and a fairness hole.

It follows story 020 (the ingress function) and story 022 (the transport bucket and drop counters), since the parity comparison covers the clamp, the duplicate, the 35-press mash and the self-ref strip. ADR-0011 notes that the host has no `NetConnection` or package map and a pawn may not have a GUID yet (E7), so the host's entry carries the aim ray's weak pawn handle directly instead of a GUID; both paths must converge to the same weak handle by the end of stage 1.

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity; the listen host's input goes through `IngestCommand(HostLocal)` with the same ingress checks and the same reply flush, with no `NM_ListenServer` special case.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011: HIGH for the input layer, MEDIUM for the command path)
**Engine Notes**: **V6** (host parity): the listen host's local commands and a remote client's identical commands produce identical stage-1 output (`FCommandPayload` minus the opaque sink). E7: the host has no `NetConnection` or package map and a pawn may not yet have a NetGUID (GUIDs are assigned lazily), which is why the host path uses a weak pawn handle. E6/V2 concerns (epoch ordering on a possession) are separate; the host epoch uses `ServerPossessionEpoch` only and never double-counts, because the two fields are separate and both run on one object.

**Control Manifest Rules (this layer)**:
- Required: The listen host's input goes through `IngestCommand(HostLocal)` with the same ingress and reply flush — source: ADR-0011
- Required: `PossessionEpoch` is server-owned and delivered only by `ClientSetPossessionEpoch` before `ClientRestart`; ingress stamps the controller's recorded `PossessedControlEpoch`, never the pawn's current control epoch — source: ADR-0011
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step in one owner-only RPC per connection — source: ADR-0011
- Forbidden: Never special-case `NM_ListenServer` in input ingress — source: ADR-0011
- Forbidden: Never carry caller identity (`IsAI`, `IsPlayer`, controller, PlayerState, team) in an entry point, payload, event or listener — source: ADR-0011
- Guardrail: Command interface: at most one reply RPC per connection per drain — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 70 (R2, Logic, U `HostIngressParity`): **GIVEN** the host-local source **WHEN** it sends a 900 cm `WorldPoint`, a duplicate seq and a mash at 35 presses per 0.1 s **THEN** clamping, ignoring and bucket drops are identical to the remote-client path; **AND** its replies are enqueued, never delivered inline.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 2 (Listen-server host), Key Interfaces and Implementation Guidelines:*

`IngestCommand(APlayerController& Owner, const FCommandOrigin& Origin, const FNetPawnCommand& Cmd)` is the single stage-1 function; the Server RPC body and the host's local path both call it. `FCommandOrigin { FMailboxId Mailbox; FReplySinkId Sink; }` is opaque. The host's replies go through the same drain-end step and are appended to the sink's per-pass buffer, flushed once at the drain end (ADR-0011 section 8), never delivered inline.

**Rationale.** A host bypass would make net-receive and host-local commands see different checks and different frame positions, which is both an `NM_ListenServer` special case and a fairness hole (GDD R2 note, design review 2026-10-24). Parity is the Pillar 1 guarantee that the host's input cannot be told apart downstream.

**V6 report (this story IS the V6 report).** Write the result to `production/qa/spikes/V6-host-ingress-parity.md` (date set when the test runs). It records for each case the compared post-ingress `FCommandPayload` bytes, that `HostLocal` has no NetConnection or package map, which weak-handle path is used, that the synthesised Seq is monotonic, and that grep for `NM_ListenServer` in input code returns 0 hits.

**GDD references.** Core Rules R2 (host paragraph) and R5.1; Edge Cases "If the listen-server host's process holds ServerOnly data" (accepted for trusted prototype playtests). Tuning Knobs (defaults): `TransportBucketSize` 30, `TransportBucketPerSec` 20, `CommandRangeSlackCm` 40.

**Gaps (QL-STORY-READY).** The AC omits what V6 actually verifies: the host's weak pawn handle versus the remote GUID path converging, including self-`PawnRef` stripping. The host synthesises its Seq, so a real host cannot send a duplicate; the test must use a test seam that calls `IngestCommand(HostLocal)` with an explicit Seq. "35 in 0.1 s" needs simultaneous arrival (see story 022; dt = 1/64 s).

**Prototype superseded.** New code, no prototype counterpart (the prototype host called the feature code directly with no ingress).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-020-ingress-clamp-and-drop.md`: the ingress rules themselves (clamp, strip, duplicate, id gate)
- `production/epics/input-interaction-framework/story-022-transport-bucket-and-abuse-log.md`: the bucket and abuse-log rules
- `production/epics/input-interaction-framework/story-026-bound-unbound-same-events.md`: hub-event parity across remote, host-local and AI paths
- `production/epics/input-interaction-framework/story-010-spike-v3-netguid-server-resolution.md`: server resolution of a NetGUID (V3)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Public/Server/Core/CommandIngress.h` (the `HostLocal` origin and the test seam)
- `Source/StealthGameplay/Private/Core/CommandIngress.cpp`
- `Source/StealthGameplay/Private/Tests/Input/HostIngressParityTest.cpp` (class `FInputHostIngressParityTest`, name `StealthGame.Input.HostIngressParity`)
- `production/qa/spikes/V6-host-ingress-parity.md` (V6 report)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- The AC omits what V6 actually verifies (the host's weak pawn handle versus the remote GUID path converging, including self-PawnRef stripping).
- The host synthesises its Seq, so a real host cannot send a duplicate.
- 35-in-0.1 s needs simultaneous arrival (see story 022).

QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "Test seam calls IngestCommand(HostLocal) with explicit Seq. Compared with the remote path, the post-ingress FCommandPayload bytes (excluding the sink) are identical for: a 900 cm WorldPoint, a duplicate Seq, 35 simultaneous Begins, and an Attack whose weak handle names the host's own pawn (stripped, counted). Host replies are enqueued, never delivered inline. The V6 report records each result."

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/HostIngressParityTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)
- Plus the V6 report `production/qa/spikes/V6-host-ingress-parity.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on:
  - `production/epics/input-interaction-framework/story-020-ingress-clamp-and-drop.md`
  - `production/epics/input-interaction-framework/story-022-transport-bucket-and-abuse-log.md`
  - cross-epic: tuning-data-config (knob registry; stories not yet written)
- Unlocks:
  - `production/epics/input-interaction-framework/story-026-bound-unbound-same-events.md`
