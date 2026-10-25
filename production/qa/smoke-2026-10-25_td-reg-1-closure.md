# Smoke check: TD-REG-1 closure (story tdc-037)

**Date**: 2026-10-25
**Story**: `production/epics/tuning-data-config/story-037-close-td-reg-1.md`
**Result**: PASS

## Precondition: the registry-match lint reports zero findings

```
bash tools/ci/lint/gate.sh tools/ci/lint/knob-lint-registry.sh
GATE: PASS (20 file(s) scanned, 0 findings)        (exit 0)
```

The lint (`tools/ci/lint/knob-lint-registry.sh`, story tdc-033) compared every owned key of the 18 owner GDDs (413 preset-field rows) with its
constant in `design/registry/entities.yaml` field by field (source GDD, default, hard range, safe range, audience, latch). Getting to zero took
two steps recorded in the earlier smoke notes: story tdc-030 added the 124 missing constants, and the first lint run then found 9 field
differences in existing constants and owner rows, which were fixed in the documents (not baselined, no rule weakened):

| Where | Finding | Fix |
|---|---|---|
| registry `CrowdMotionProfileAsset`, `CrowdFullStateTreeAsset`, `CrowdRestrictedStateTreeAsset` | hard range text on the owner row, absent in the registry | `hard_range` added with the owner row's text |
| registry `CrowdDecoyEarlyReleaseMaxSec` | latch `Round`; the owner row gives the Min/Max pair the one latch `Action` | latch set to `Action` |
| registry `ReachDetourMult` | value 2.0, hard 1.6-2.4, safe `= DetourMaxMult`; the LSC row says derived alias, hard `follows DetourMaxMult (1.8-2.4)` | registry follows the owner row |
| registry `TeammateMarkerLargeMult` | hard range wording differed from the owner row | registry follows the owner row |
| `design/gdd/level-socket-contract.md` row `SunPoolBeaconRangeCm` / `bSunPoolBeaconIsReject` | one hard-range cell `3,000-6,000` covered a float key and a bool key | cell made explicit: `3,000-6,000 / true, false` |

## Register change

`docs/tech-debt-register.md` row TD-REG-1: Status `Open (opened 2026-10-25)` became `Resolved 2026-10-25` citing story 037, this smoke check and the
lint result. The row also carries the change request to the TDC GDD owner: OQ-15 is answered, closed by story 037 (proposed wording for the owner;
the GDD is not edited). The new row TD-REG-2 (bounded-by baseline, story tdc-036) is a separate item and stays Open.

## TDC GDD unchanged

`design/gdd/tuning-data-config.md` is byte-identical to `HEAD`: `git hash-object design/gdd/tuning-data-config.md` and
`git show HEAD:design/gdd/tuning-data-config.md | git hash-object --stdin` both give `96554152523d76c5c726258f1db23721fa30242a`;
`git diff --stat -- design/gdd/tuning-data-config.md` is empty.

## Known observation left for the GDD owner (not a TD-REG-1 item)

The parser (`python -I tools/ci/lint/knob_tables.py`) lists one count discrepancy: TDC's Registered Owner Keys table says the Level Socket Contract
registered 80 keys, its Tuning Knobs table has 84 owned rows. The TDC GDD is not edited by this story, so it stays an observation.
