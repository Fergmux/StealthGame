# Story 047: Every leak catalogue row L1-L34 has an owner, a status and the right citation

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Config/Data
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

The Pillar-1 leak catalogue (GDD R12, rows L1-L34) is only useful if each row is owned, has a status and points at the thing that backs the status: a rule or acceptance criterion for mitigated rows, a spike ID for pending rows, a change request for required rows. Accepted rows must be visible in the release checklist as "not shippable beyond trusted playtests". This story turns the GDD table into a checked-in status table with release-checklist rows, and records the review. It is a document review gate; it feeds the CRITICAL ship gate script in story-049.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-007` (Leak catalogue with CRITICAL ship gate; per-commit canaries)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (section 8, release / milestone gate row; section 3)
**ADR Decision Summary**: ADR-0008 section 8 makes the AC-IVP-33 and AC-IVP-44 document gates part of the release and milestone gate: before any build leaves the team (invited, evidence and public builds), no CRITICAL row may be `Spk` or `Req`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No engine API is used; this is a document review. The statuses cite engine-dependent spikes (S1, S2, S9, S10, S13, S14) whose [U] claims are listed in ADR-0001 and ADR-0003 Engine Compatibility.

**Control Manifest Rules (this layer)**:
- Required: **Every new replicated field, fact, audience, condition or replicated class ships with a Visibility Policy row and a signed-off allowlist row**; the reflection audit fails the build otherwise — source: ADR-0001, ADR-0003
- Forbidden: **Never hide concealment client-side only or replicate a concealed flag** (internal development switch only); never use dormancy or distance culling for concealment — source: ADR-0003
- Required: **The game NetDriver runs the legacy replication system and the harness asserts it at startup** — source: ADR-0001

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, AC-IVP-33, scoped to this story (retyped Config/Data: recorded doc review plus a smoke-check line):*

- [ ] **AC-IVP-33** [Doc] `StealthGame.Visibility.LeakCatalogueReview`: GIVEN rows L1-L34, WHEN reviewed at each milestone gate, THEN every row has an owner and status; Mit rows cite a rule or AC, Spk rows a spike ID, Req rows a change request; Acc rows (L11, L14, L19-L21, L23; L29 dev builds only) appear in the release checklist as "not shippable beyond trusted playtests" (R10 definition: no public demo, festival build or open test while L14 or L21 is Accepted). (R12)

---

## Implementation Notes

*Derived from ADR-0008 section 8 and the GDD R12 table (rows L1-L34, status vocabulary Mit / Spk / Acc / Req):*

- Create the checked-in status table at `production/qa/leak-catalogue-status.md` (location chosen by this story): one row per L1-L34 with columns row, surface, owner, status, and the citation that backs the status (rule or AC for `Mit`, spike ID for `Spk`, change request for `Req`). Copy owners and statuses from the GDD R12 table; the table is the working copy, the GDD stays the source of the mitigation text. Include a "Release checklist rows" section in the same file: one row per `Acc` item (L11, L14, L19, L20, L21, L23, and L29 for dev builds only) reading "not shippable beyond trusted playtests", with the R10 definition (no public demo, festival build or open test while L14 or L21 is Accepted).
- The table must also mark the CRITICAL rows (L1, L2, L4, L13, L29, L30, L31) so story-049's gate script can read them, and keep status as a single token (`Mit`, `Spk`, `Req`, `Acc`) in a fixed column so it can be parsed. Keep the format simple; the gate script is the one that decides pass or fail (observation-only rule).
- Mixed statuses in the GDD (for example L4 `Mit (spawn); Spk S10 (re-open)`, L10 and L24 with accepted residuals) need a decision recorded in the table: state which token the row carries for the CRITICAL check and keep the residual note. The AC lists `Acc` rows as L11, L14, L19-L21, L23 and L29 (dev builds only); L10 and L24 carry accepted residuals but are `Mit` rows, so they are not in that list (record this as the review's reading).
- Review record: `production/qa/evidence/leak-catalogue-review-evidence.md` holds the reviewer, date, the 34 row results and the milestone-gate it was run for. Re-run the review at each milestone gate. Add a smoke-check line in the next `production/qa/smoke-*.md` stating the review file and its result.
- Prototype: not applicable. Replaces: none (new).
- No cross-epic dependency beyond the owners named in the table (their GDDs supply the citations).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-049: the release-checklist gate script that reads this table (no CRITICAL row `Spk` or `Req`, `bConcealServerCull` true, AC-IVP-37 passed)
- story-048: the playtest session-record gate
- story-046: the Iris rerun gate
- the spikes and ACs that move rows from `Spk` to `Mit` (stories 001-008 and their dependent AC stories)

---

## Files

*Planned:*

- `production/qa/leak-catalogue-status.md` (status table L1-L34 and release-checklist rows)
- `production/qa/evidence/leak-catalogue-review-evidence.md` (recorded review)

---

## QA Test Cases

- **AC-1**: AC-IVP-33 `StealthGame.Visibility.LeakCatalogueReview` (doc review, manual)
  - Setup: the checked-in leak-catalogue status table (rows L1-L34).
  - Verify: every row has an owner and a status; Mit rows cite a rule or AC, Spk rows a spike ID, Req rows a change request; Acc rows (L11, L14, L19-L21, L23; L29 dev builds only) appear in the release checklist as "not shippable beyond trusted playtests".
  - Pass condition: all 34 rows pass; the review is recorded at each milestone gate.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are waived, but a retained review record is still required here.*

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass (`production/qa/smoke-*.md`) with a line for this review, plus the recorded review under `production/qa/evidence/leak-catalogue-review-evidence.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None
- Unlocks: `production/epics/information-visibility-policy/story-049-external-build-critical-gate.md`
