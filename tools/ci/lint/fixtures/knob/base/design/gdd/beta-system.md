# Beta System

## Tuning Knobs

Registered under Tuning Data & Config CR-15; owner Beta System for every row. **Audience** `ServerOnly` for every key. **Latch** `Round`.

| Key | Type | Default | Hard range | Safe range | Unit | Effect / breaks if too high or low |
|---|---|---|---|---|---|---|
| `BetaWindowSec` | float | 3 | 1-10 | 2-6 | Sec | The window. Bounded below by Alpha I-AB-1. High: idle. Low: premature. |
| `BetaLimitCount` | int | 5 | 1-9 | 3-6 | Count | A limit; bounded by I-AB-2. |
| ~~`BetaOldSec`~~ | float | 1 | 1-2 | 1-2 | Sec | Retired. |
| `AlphaMaxCount` (reference) | int | 8 | 4-16 | 6-10 | Count | **Owned by Alpha System** (one quantity, one name); listed for reference, not registered here. |
