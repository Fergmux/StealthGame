# Alpha System

## Overview

A fixture GDD for the knob-table selftests (not a real system).

## Tuning Knobs

Knobs the Alpha System owns. The owner is Alpha for every row.

| Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect / breaks if too high or low |
|---|---|---|---|---|---|---|---|---|---|
| `AlphaHoldSec` | float | Alpha | 2.0 | 1-6 | 1.5-4 | ServerOnly | Round | Sec | Hold time; bounded by I-AB-1 (it stays below `BetaWindowSec`). Low: flicker. High: slow rounds. |
| `AlphaMinCount` / `MaxCount` | int | Alpha | 4 / 8 | 2-10 / 4-16 | 3-5 / 6-10 | ServerOnly | Round | Count | Pair row: the Min and the Max of one range (I-5); the Min is bounded by I-AB-2. |

A second table in the same file, columns reordered and one extra column.

| Key | Default | Type | Owner | Notes | Hard range | Safe range | Audience | Latch | Unit | Effect / what breaks |
|---|---|---|---|---|---|---|---|---|---|---|
| `AlphaScaleMult` | **60.0** (provisional) | float | Alpha | extra | 30-90 | 50-70 | All (prompts) | Use | Mult | Scale; bounded by I-1. Low: small. High: large. |

Knobs it consumes (owned elsewhere, never duplicated here):

| Key | Owner | Default | Alpha use |
|---|---|---|---|
| `BetaWindowSec` | Beta System | 3 | The window the hold must stay inside. |
