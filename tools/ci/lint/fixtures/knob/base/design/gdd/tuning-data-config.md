# Tuning Data & Config

## Detailed Design

### Registered Owner Keys (reconciliation)

| Owner GDD (index #) | Keys | Audience | Latch | Owner invariants | Notes |
| ---- | ---- | ---- | ---- | ---- | ---- |
| Alpha System (1) | 4 (incl. `AlphaScaleMult`; paired row counted per name) | ServerOnly | Round | I-AB-1 | |
| Beta System (2) | 2 | ServerOnly | Round | I-AB-2 | |
| Tuning Data & Config (3) | 1 | ServerOnly | -- | I-1 | |

## Formulas

**Invariants** (evaluated on clamped values; failure rejects the whole preset):
- **I-1** `AlphaScaleMult >= 1`. Rationale only.
- **I-5** For every registered range pair (`...MinCount`/`...MaxCount`): `Min <= Max`.

| ID | Owner | Constraint (clamped values) | On failure |
| ---- | ---- | ---- | ---- |
| I-AB-1 | Alpha | `AlphaHoldSec < BetaWindowSec` | reject |
| I-AB-2 | Beta | `BetaLimitCount >= AlphaMinCount` | reject |
| O-AB-3 | Alpha | `AlphaHoldSec * 2 <= TdcSelfSec` | offline (CI) |

## Tuning Knobs

| Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect / what breaks |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| `TdcSelfSec` | float | TDC | 10 | 5-60 | 8-20 | ServerOnly | Round | Sec | The TDC fixture knob. |
