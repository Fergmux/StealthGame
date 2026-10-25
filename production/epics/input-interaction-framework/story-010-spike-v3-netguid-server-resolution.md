# Story 010: Spike V3 reports server NetGUID resolution of a culled pawn

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**Other ADRs**: ADR-0003: Replication, relevancy and the concealment cull hybrid
**ADR Decision Summary**: An Attack's `PawnRefGuid` travels as an `FNetworkGUID` value and is resolved at ingress through the NetDriver's GUID cache to a weak pawn handle; resolution succeeds even for a pawn culled for that connection, so it grants nothing: visibility is decided only by the `CanViewerKnow` and `PawnRefAgreementCm` gates. ADR-0003 supplies the concealment cull whose effect on resolution V3 measures.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Highest of ADR-0011 (HIGH) and ADR-0003 (HIGH). This story IS verification item V3 of ADR-0011: "server-side resolution of an Attack `PawnRef` NetGUID through the NetDriver's GUID cache (E7) returns the live pawn for any GUID the server assigned, including a pawn culled for that connection, so the `CanViewerKnow` and `PawnRefAgreementCm` gates alone decide visibility; an unassigned or stale GUID returns null." Engine fact E7: `UPackageMapClient::GetObjectFromNetGUID(const FNetworkGUID&, bool bIgnoreMustBeMapped)` defers to the GUID cache, which on the server belongs to the NetDriver, not the connection; GUIDs are assigned lazily, so a pawn never sent to any client may have none. Source: `PackageMapClient.h:235, 526`; `PackageMapClient.cpp:2760-2762`.

**Control Manifest Rules (this layer)**:
- Required: Human Move/Look travel only in `ServerMove`; discrete commands only in `ServerPawnCommand`; `PawnRef` travels as a NetGUID value — source: ADR-0011
- Required: The Attack payload comes only from Camera's `GetAimRay()`; `PawnRef` is honoured only through `CanViewerKnow` and `PawnRefAgreementCm` — source: ADR-0011, ADR-0015
- Forbidden: Never treat a resolved `PawnRef` as evidence of visibility — source: ADR-0011
- Forbidden: `PawnRef` is never an object-pointer field — source: ADR-0011
- Guardrail: Ingress is O(1) per RPC; GUID resolution is one cache lookup — source: ADR-0011

This is a verification spike. The privacy argument for `PawnRef` is that resolving a GUID on the server reveals nothing: the server could resolve a culled pawn's GUID anyway, so the only visibility test is `CanViewerKnow` plus the agreement distance (GDD R10). If instead resolution failed for a culled pawn, an Attack naming a culled pawn would behave differently from one naming a visible pawn at ingress, an observable difference. V3 measures the engine's behaviour so story 061 (`PawnRefSameAsSpot`), 062 (stale `PawnRef` falls back) and 063 (`PawnRefGate`) can rest on it. This story does not block any story in the plan (its report conditions ADR-0011 only), but a FAIL means the ingress design for `PawnRef` must be revisited by a superseding ADR.

It has no story dependency. The culling mechanism is ADR-0003's and may not exist yet; the qa-lead notes the spike therefore uses `IsNetRelevantFor` returning false for the test connection and records which mechanism it used. Host (no package map) parity is V6, story 024, not here.

---

## Acceptance Criteria

*ADR-derived spike. Source: ADR-0011 Engine Compatibility, Verification Required item V3; criterion text from the plan:*

- [ ] **GIVEN** a server with a pawn relevant to a test connection and a pawn culled for that connection **WHEN** server `FNetGUIDCache::GetObjectFromNetGUID` is asked to resolve (a) a GUID the server assigned to a relevant pawn, (b) a GUID of a pawn culled for that connection, (c) a never-assigned GUID, (d) a stale GUID of a pawn destroyed earlier and (e) a GUID with the client-assigned bit pattern **THEN** a report at `production/qa/spikes/` records the resolved object or null per case, showing (a) and (b) resolve to the pawn and (c), (d), (e) resolve to null with no crash or log error above Warning, and states PASS only if all five match.

---

## Implementation Notes

*Derived from ADR-0011 Decision §3.1 step 4, Engine Compatibility V3 and Implementation Guidelines:*

Ingress step 4 (§3.1): "`Attack`: `WorldPoint` clamped (not dropped) to `AssassinationRangeCm + CommandRangeSlackCm` of the pawn; a `PawnRefGuid` naming the commanding pawn is stripped and counted; the `PawnRefGuid` is resolved through the NetDriver's GUID cache (E7) to a weak pawn handle (unresolvable to absent). Resolution succeeds even for a pawn culled for that connection, so it grants nothing: visibility is decided only by the `CanViewerKnow` and `PawnRefAgreementCm` gates (§9)."

**Spike procedure (from the qa-lead spec):** record the engine source function used (server `FNetGUIDCache::GetObjectFromNetGUID`, with its `bIgnoreMustBeMapped` argument), and per case the resolved object or null: (a) GUID the server assigned to a pawn relevant to the connection; (b) GUID of a pawn culled for that connection (channel closed); (c) never-assigned GUID; (d) stale GUID of a pawn destroyed earlier; (e) GUID with the client-assigned bit pattern. Pass rule: (a) and (b) resolve to the pawn; (c), (d), (e) resolve to null with no crash or log error above Warning. The report states PASS only if all five match, and records that the resolved pawn is never itself evidence of visibility (`CanViewerKnow` decides). Edge cases: two connections sharing one NetDriver cache (resolves for both); GUID reuse after destroy. Record which culling mechanism the spike used (`IsNetRelevantFor` false for the test connection until ADR-0003's cull exists). The host (no package map, weak handle path) is V6, story 024.

Prototype: new code, no prototype counterpart (the prototype's `ServerAssassinate(Target, ...)` trusted a client-named object). Tuning keys: `PawnRefAgreementCm` (75, owner IIF) is context only, not exercised here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-024-host-ingress-parity.md`: V6, the listen-host weak-handle path.
- `story-030-attack-never-rejected-by-target.md`: an Attack is never rejected by the target (AC 11).
- `story-061-pawnref-same-as-spot.md`, `story-062-stale-pawnref-falls-back.md`, `story-063-pawnref-gate.md`: the production `PawnRef` handling and fallback.
- `story-020-ingress-clamp-and-drop.md`: the production ingress that uses the resolution.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `production/qa/spikes/V3-netguid-server-resolution-report.md` (the report; final dated name chosen at write time)
- `Source/StealthTestHarness/Private/SpikeV3NetGuidServerResolutionTest.cpp` (throwaway probe harness; not a gate test)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Integration — spike specification** (QL-STORY-READY: ADEQUATE; note: the culling mechanism is ADR-0003's and may not exist yet; the spike uses `IsNetRelevantFor` returning false for the test connection and records which mechanism it used):

- **AC-1**: Spike V3 reports server NetGUID resolution
  - Given: a server with a relevant pawn and a culled pawn for the test connection; the report records the engine source function used (server `FNetGUIDCache::GetObjectFromNetGUID`, with its `bIgnoreMustBeMapped` argument).
  - When: resolving (a) a GUID the server assigned to a pawn relevant to the connection; (b) the GUID of a pawn culled for that connection (channel closed); (c) a never-assigned GUID; (d) a stale GUID of a pawn destroyed earlier; (e) a GUID with the client-assigned bit pattern.
  - Then: (a) and (b) resolve to the pawn; (c), (d), (e) resolve to null with no crash or log error above Warning. The report states PASS only if all five match, and records that the resolved pawn is never itself evidence of visibility (`CanViewerKnow` decides).
  - Edge cases: two connections sharing one NetDriver cache (resolves for both); GUID reuse after destroy.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the spike report `production/qa/spikes/V3-netguid-server-resolution-report.md` (a documented result, retained) OR the probe harness test `Source/StealthTestHarness/Private/SpikeV3NetGuidServerResolutionTest.cpp`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None (cross-epic: shared-character-locomotion pawn and information-visibility-policy cull mechanism, stories not yet written; tuning-data-config StealthTestHarness module)
- Unlocks: None
