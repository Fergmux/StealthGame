# Tech Debt Register

| ID | Area | Description | Origin | Planned resolution | Status |
|----|------|-------------|--------|--------------------|--------|
| TD-REG-1 | Design registry | About 130 owner keys outside the Level Socket Contract are not mirrored in `design/registry/entities.yaml` (the owner GDD tables remain the source of truth for them). | `/review-all-gdds` 2026-10-25b warning W1; ruling D30 in `production/design-closeout/2026-10-25_rereview-followups.md` | Resolved in the TDC validator story: the AC-3b doc lint is built there and registers and validates the missing keys as its first task (see `design/gdd/tuning-data-config.md` OQ-15). | Open (opened 2026-10-25) |
