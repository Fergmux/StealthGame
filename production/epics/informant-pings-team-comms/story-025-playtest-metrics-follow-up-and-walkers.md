# Story 025: Playtest metrics: Fellowship follow-up and walker coverage

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (AC-PNG-13, OQ1, OQ6, OQ7 with ruling D34, OQ8)
**Requirement**: `TR-png-023`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0023: Playtest telemetry hooks (§3 what is recorded per question, §6 export, §7 analysis side)
**ADR Decision Summary**: Playtest questions are answered by server-side telemetry recorded in a separate section of the Event Log store and exported after termination; analysis happens offline on the export, and nothing is recorded client-side or per person in a client-visible form.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (ADR-0023; offline analysis over exported data)
**Engine Notes**: No engine API: the analysis is an offline function over the ADR-0023 export (a script or a plain C++ commandlet in the editor module; ADR-0023 §7 says which). The fixtures are checked-in data files, so the tests are deterministic.

**Control Manifest Rules (this layer)**:
- Required: the analysis reads only the ADR-0023 export; placements and ally positions are already exported — source: ADR-0023
- Forbidden: Never export `DealDigest`, mailbox order, sub-seeds, display names, identity keys, addresses, ping or per-participant `AppearanceId` — source: ADR-0023 (so the export carries placement and role class only; story 024)

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (AC-PNG-13, OQ7 with D34), revised at QL-STORY-READY. Gate: ADVISORY until the first MVP playtest:*

- [ ] **AC1** (AC-PNG-13): **GIVEN** a telemetry fixture of informant pings, ally positions and counts **WHEN** the analysis runs **THEN** it reports pings per informant-minute and the share of informant pings followed within 10 s (inclusive) by an ally's pawn (not the sender) entering 8 m (inclusive) of the ring centre; >= 40 % passes (placeholder target), 20.0 % to 39.9 % is reported as inconclusive, below 20 % flags OQ1 and OQ8; the fixture asserts 39.9 %, 40.0 %, 20.0 % and 19.9 % (as integer counts, for example 399/1000) and a follow-up at exactly 10.0 s and 8.0 m.
- [ ] **AC2** (OQ7, D34): **GIVEN** a fixture of ping chains (same sender, each ring within 4 m of its previous ring and <= 3 s apart) **WHEN** analysed **THEN** the ring-seconds of a chain within any sliding 60 s window are the sum over its pings of `min(PingLifetimeSec, time to the sender's next ping or the window end)`; a chain is flagged when that exceeds 30 % of the window (18 s) and a round-set is flagged when more than 1 in 5 rounds are flagged; the fixture asserts 17.9 s (not flagged), 18.0 s (not flagged, not above 30 %) and 18.1 s (flagged), and 1 of 5 rounds (not flagged) and 2 of 5 (flagged); the report names `PingBucketPerSec` (with I-PNG-1 held) as the lever; a 4.0 m link and a 3.0 s link are in the chain, 4.1 m and 3.1 s break it.
- [ ] **AC3** (ADR-0023, advisory status): **GIVEN** the analysis **WHEN** run on a fixture holding only the ADR-0023 export fields **THEN** it succeeds without any new client-side probe, the report row is marked ADVISORY until the first MVP playtest, and a fixture that needs an extra required field fails the test.

---

## Implementation Notes

*Derived from GDD AC-PNG-13, OQ6, OQ7 (D34) and ADR-0023 §3, §7:*

Pings carry no pawn identity (PNG-R1), so a "walker" is observable only as the chain: consecutive rings of one sender, each within 4 m of its previous and no more than 3 s apart. A replacement truncates the previous ring (one slot per sender, D25), which is why ring-seconds use the time to the sender's next ping. Window is a sliding 60 s window over the round; a round is flagged if any chain exceeds the threshold in any window. Zero informant pings must produce a "not assessed" row, not a division error. Outcome mapping stays advisory: the verdict words are for the playtest report (`/playtest-report`), the first MVP playtest records the real values; D34 is the only authoritative threshold text (`production/design-closeout/2026-10-25_rereview-followups.md`).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 024: the probes and the export's ping fields.
- The first MVP playtest session itself and its report (the `/playtest-report` workflow).
- OQ8's decision to restrict pings to informants (a creative-director decision after the playtest).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameEditor/Private/StealthPingAnalysis.h` and `StealthPingAnalysis.cpp` (analysis over the export; or the script path named by ADR-0023 §7)
- `Source/StealthGameEditor/Private/Tests/Pings/PingAnalysisTest.cpp` (class `FPingsAnalysisTest`, name `StealthGame.Pings.Analysis`)
- `Source/StealthGameEditor/Private/Tests/Pings/Fixtures/` (checked-in telemetry fixtures)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 follow-up metric**
- Given: a telemetry fixture of informant pings, ally positions and counts (integer counts such as 399/1000).
- When: run the analysis.
- Then: reports pings per informant-minute; 40.0 % passes, 39.9 % does not; 19.9 % flags OQ1 and OQ8; 20.0 % is inconclusive; the ally excludes the sender; 10.0 s and 8.0 m count.
- Edge cases: zero informant pings means no division error and the report row says not assessed.

**Test: AC2 walker-coverage flag**
- Given: chain fixtures (same sender, each ring within 4 m of its previous and <= 3 s apart).
- When: analyse.
- Then: flags per the ring-seconds rule (17.9 s no, 18.0 s no, 18.1 s yes; 1 of 5 rounds no, 2 of 5 yes); names `PingBucketPerSec` (with I-PNG-1 held) as the lever.
- Edge cases: a 4.0 m link and a 3.0 s link are in the chain; 4.1 m and 3.1 s break it.

**Test: AC3 export-only and advisory**
- Given: the analysis and a fixture containing only the ADR-0023 export fields.
- When: run.
- Then: it succeeds without any new client-side probe; the report row is marked ADVISORY.
- Edge cases: adding a required extra field makes the test fail.

QL-STORY-READY: GAPS (revised) — "walker coverage", the window type, the ring-time rule, the fixture numbers and the inclusive bounds were pinned as above on the qa-lead's proposals.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameEditor/Private/Tests/Pings/PingAnalysisTest.cpp` OR a documented playtest (ADVISORY gate)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-024-ping-telemetry-statistics.md`
- Unlocks: None
