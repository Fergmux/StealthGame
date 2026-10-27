# Evidence: story asn-026 Advisory checks: probe economics (F5), probe and brace telemetry, resolver cost

Story: `production/epics/assassination-defence-reversal/story-026-advisory-economics-telemetry-resolver-cost.md`
Date: 2026-10-27. Build: `tools/dev/cycle.ps1 -Tests -NoLaunch`, `Result: Succeeded`; full `StealthGame.` run 391 pass, 2
fail (story asn-022's PIE tests, fixed and re-run 71 / 71 in the Assassination set). Measurements: `measurements.txt`;
files stamped in `stamp.txt`. All three ACs are ADVISORY.

| AC | Test | Result |
|---|---|---|
| AC1 (AC-ASN-32) | `StealthGame.Assassination.ProbeEconomicsF5` | p 0.0909: E[N] 2.9761, E[RivalKills] 0.2758, P(>=1 kill) 0.2317, P(strike-out) 0.9762, P(reversed) 0.0238, 10.70 NPC kills per rival kill; edges p = 0, p = 1, L = 1, b_P = 0; TDC BlindGuessSuccess / ExpectedBlindKills +7.3 % / +8.8 % optimistic at the defaults (safe direction for I-4), +7.3-9.2 % at TeamSize 4 / NpcCount 30 and 5 / 80 / L 4. PASS. |
| AC2 (AC-ASN-33) producer | `StealthGame.Assassination.ProbeAndBraceTelemetryProbes` | One StrikeResolved per kill / reversal / whiff in its slot (refs only), the reversed defender's brace age = contact - start and not ready, BraceEnded Reversal / Expired / Removed; the null sink path resolves identically; the cost meter counts. PASS. |
| AC2 report | `python tools/analysis/assassination_telemetry_report.py --self-test` | The synthetic fixture (3 terminated rounds, 2 matches, 1 unterminated skipped, a pair trade at exactly 10.0 s counted and one at 10.01 s not, a match with 0 rival kills) reproduces `expected-report.md` exactly; watched failing with the window set to 9.99 s. PASS. |
| AC2 live pull | — | DEFERRED (advisory): needs the Event Log's ADR-0023 telemetry section (Round / Foundation epics; production passes the null sink until it exists) and >= 10 logged internal matches. |
| AC3 (AC-ASN-34) | `StealthGame.Assassination.ResolverCost` | 50 pawns, 10 Attacks per frame through the registered resolver, median over 100 frames after 30 warm-up: 10 kills 0.0057 ms, 10 reversals 0.0070 ms, 10 on one victim 0.0036 ms (cold first frame <= 0.021 ms), all <= 0.1 ms; per-strike payload 44 bytes (FActionState + StrikeResult reply entry) <= 64. Development editor smoke, not the cooked 3060-class evidence standard. PASS (advisory). |

Run result: N/A — server-only probes, an offline report and a cost measurement: nothing is player-visible (ADR-0023: no
replication, no HUD, nothing reads telemetry back).
