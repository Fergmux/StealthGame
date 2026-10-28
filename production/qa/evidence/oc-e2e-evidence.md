# End-to-end Cacao chain evidence (Objectives story oc-021 AC2) - 2026-10-28

Story: `production/epics/objectives-chain/story-021-pie2-end-to-end-cacao-chain.md`. Run: 3-player listen-server PIE on L_Prototype
(50 NPCs; team 0 = host + Client 2 "Jaguar", team 1 = Client 1 "Eagle"), the production chain with real holds (`StealthBot`
interact), the HUD's stage card (the HUD epic's card is not built; this is the prototype HUD panel reading only the own team view).

| File | Moment | What it shows |
|---|---|---|
| `oc-e2e-card-p1.png` | team 0 client at Active | "Stage 1/3: Pay the Runner - A court among the houses, by the shrine [hold E]", Rival steps 0 of 2 |
| `oc-e2e-card-p2.png` | team 0 after its P1 | "Stage 2/3: Gather the Root - A court among the houses, by the water steps" |
| `oc-e2e-final-card.png` | team 0 after its P2 | "Stage 3/3: The Ruler's Cacao - A court on the causeway, by the shrine" |
| `oc-e2e-result.png` | team 0 client at the result | "JAGUAR WIN (final objective)", summary Jaguar steps 3 of 3, Eagle 1 of 3; Rival steps 1 of 2 |
| `oc-e2e-result-team1.png` | team 1 client at the result | the same result; team 1 still at "Stage 2/3: Take the Gourd", never a final card; Rival steps 2 of 2 |

The card titles are the Cacao rows of the OC-16 table (DA_ObjectiveChain_Cacao). The clue phrases come from the role-blind stand-in
(oc-013). Server log excerpt: `objectives-chain-013-023-2026-10-28/pie-run-and-observe-log.txt` ("Round 1 decided by the production
rules: team 0 wins (public reason 1, end reason 0)"). The automated assertions (FRoundResult, 4 ObjectiveStage + 1 FinalCompleted in
key order, SP-1 on team 1's client) are `StealthGame.Objectives.PIE.EndToEndCacaoChain`.

Sign-off: agent run-and-observe 2026-10-28; human review PENDING.
