# ADR-0006: Seeds and deterministic/secret random streams

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed (awaiting user acceptance)

## Date
2026-10-24

## Last Verified
2026-10-24 (against UE 5.8.4 headers at `C:/Program Files/Epic Games/UE_5.8/Engine/Source` and the 2026-10-24 reconciled Session & Travel, Character Pool, Procedural City Generator, Crowd, Round and Objectives GDDs)

## Decision Makers
Design pipeline (autonomous run, `/architecture-decision --review full`); technical-director (TD-ADR) and Unreal engine-specialist validation folded into one review; user acceptance pending.

**Review record (2026-10-24):** TD-ADR **CONCERNS** (9: include-lint rule missing from ADR-0007; header names not `Stealth`-prefixed; related-key stream keying and `Draw`/`Block` overlap; Shipping misuse path inert and scope not thread-safe; `AmbientSchedule` shape ambiguity; OC-12 seconds vs no-float rule; TR traceability and implied change requests; listen-host threat model; ranged-draw edge cases and decoration seeding). Engine validation **MINOR NOTES** (FRandomStream state needs ~2 outputs; BCrypt include/link pattern; `CreateGuid` is UUIDv7 on Unix; PlatformCrypto is enabled-by-default, not Beta — rejection reason corrected; `ensure`/logging inert in Shipping; multicharacter tag literals; PCG32 seeding routine; `-rot` C4146; `static_assert` Mix32 vectors). **All revised in place** (autonomous run, recommended option). Gates not re-run on the revised text; `/architecture-review` in a fresh session is the next independent check.

## Summary
Smoking Mirror needs two kinds of randomness that must never mix: a **public** seed that every client may know and that must rebuild the identical city on every machine, and **secret** server-side draws (appearance deal, spawn order, intake permutation, crowd behaviour, emergence and respawn picks, rival signal delays) that a modified client must not be able to predict or rewind. This ADR fixes the primitives: a 128-bit `ServerSecret` from the OS CSPRNG; an in-project **SipHash-2-4** keyed PRF as Session F5's `H64` for the round seed and every server sub-seed; a counter-mode **secret stream** built on the same PRF for every hidden draw (the keyed-hash DRBG Character Pool requires); Session's reference **Mix32** for public sub-seeds; and an integer-only **PCG32** stream with multiply-shift rejection for the Generator and client decoration — never `FRandomStream`, `FMath::Rand` or floats.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4) |
| **Domain** | Core (math, platform entropy) |
| **Layer** | Foundation |
| **Knowledge Risk** | LOW. The decision depends on no engine API beyond plain integer arithmetic and one platform entropy call; every primitive is in-project code with published reference vectors |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `breaking-changes.md`, `deprecated-apis.md` (silent on RNG); 5.8.4 headers: `Core/Public/Math/RandomStream.h` (`FRandomStream`), `Core/Public/Hash/Blake3.h`, `Core/Public/Hash/Fnv.h`, `Core/Public/Hash/CityHash.h`, `Core/Public/GenericPlatform/GenericPlatformMisc.h:1119` (`CreateGuid`), `Plugins/Experimental/PlatformCrypto/Source/PlatformCryptoContext/Public/EncryptionContextOpenSSL.h:348-357` |
| **Post-Cutoff APIs Used** | None |
| **Verification Required** | Golden vectors on every CI platform and compiler (MSVC now; clang when a Linux dedicated server arrives, ADR-0024): SipHash reference vectors, PCG32 reference outputs, Session F5 Mix32 vectors (`Mix32(123456789,1,0) = 2107646860`, `(…,1,1) = 523860287`, `(…,2,0) = 313478722`), Seed derivation fixture with a fixed test secret, Generator golden seeds (ADR-0008). Entropy call returns success on every target. |

**Engine facts used:**

| Fact | Tag | Evidence |
|---|---|---|
| `FRandomStream` is a 32-bit LCG (`Seed = Seed * 196314165 + 907633515`) and derives integer ranges from a **float** fraction (`RandHelper(A) = TruncToInt(GetFraction() * float(A))`) | [V] | `RandomStream.h:186-189, 362-364` |
| `UE::HashStringFNV1a` is a string hash; `FBlake3` exposes no keyed-hash mode in its public API; `CityHash` is unkeyed | [V] | `Fnv.h`, `Blake3.h:83-92`, `CityHash.h` |
| `FPlatformMisc::CreateGuid` exists (Windows: OS GUID); not documented as a CSPRNG | [V] exists / [U] entropy quality | `GenericPlatformMisc.h:1119`, `WindowsPlatformMisc.cpp:1547` |
| `PlatformCrypto` (in the `Experimental` folder, but `EnabledByDefault: true`, not Beta) offers `CreateRandomBytes` = OpenSSL `RAND_bytes` | [V] | `EncryptionContextOpenSSL.h:348-357`, `.cpp:721-724`; `PlatformCrypto.uplugin:13-17` |
| Windows `BCryptGenRandom(nullptr, …, BCRYPT_USE_SYSTEM_PREFERRED_RNG)` is the OS CSPRNG (Win32 API, not engine) | [U] in-project use; standard Win32 | spike-free; unit test asserts success |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0005 (`ServerSecret`, `MatchNonce` and `UsedSeeds` live in `FSessionCore`; drawn at `OpenSession`, cleared at `EndMatch`; `FRoundParams` carries the public `Seed`) |
| **Enables** | ADR-0002 (per-tick `SeqOrder` / `TieRank` permutations), ADR-0012 (Character Pool `Deal`/`Replace`/`Take` streams), ADR-0014 (Generator integer stream, stage streams, golden seeds; client decoration), ADR-0016 (crowd counter-based draws), ADR-0019 (`RespawnPick`), ADR-0020 (`StageSignal`), ADR-0008 (golden and statistical tests) |
| **Blocks** | Every story that draws a random number: Session seed draw, Character Pool deal, Round spawn order and intake, Layout selection and emergence, Generator, Crowd behaviour, Objectives rival signal |
| **Ordering Note** | Accept with or after ADR-0005. Golden vectors are committed with the first implementation story and never regenerated silently. |

## Context

### Problem Statement
Session F5 fixes the *contract* (encodings, purpose tags, the Mix32 reference mixer and its golden vectors) and leaves three choices to an ADR: the keyed 64-bit hash `H64`, the DRBG primitive Character Pool's `Replace`/`Take` streams must use ("a plain invertible PRNG such as PCG32 is not acceptable", CP-9), and the Generator's integer stream ("FRandomStream-style state, `uint32` only; -> ADR for the exact generator", R-G3). Several consumers also specify draw shapes (unbiased Fisher-Yates with rejection sampling, CP-8; counter-based crowd draws independent of kills, CNB-R14; per-tick permutations, Round R12; a fresh per-event delay keyed by a completion ordinal, OC-12). Without one decision each owner would pick its own RNG, and two failure modes follow: a public value seeding a hidden decision (the prototype places objective sites from the public layout seed, so a modified client that replays the generator learns where the objectives are), and a secret stream whose outputs (observable NPC behaviour or replacement looks) let a cheater recover its state and predict which bodies are not NPCs.

### Constraints
- **Public/secret separation is a Pillar-1 property** (Session rule 14, architecture principle 5): the public `Seed` may drive only geometry and the socket set; every hidden decision draws from server-only sub-seeds that are independent of `Seed`.
- **Cross-platform determinism for public draws:** clients do not run the Generator (they build from `FLayoutManifest`, Session OQ6), but client decoration does draw from `PublicSubSeed(Layout, LayoutAttempt)` + `LayoutHash`, and Generator golden seeds must be stable across CPUs and compilers (R-G3.4, TR-citygen-002).
- **Secret draws must be unpredictable and non-invertible** from observed outputs (CP-9 keyed-hash DRBG; CNB-R14 behaviour; Round R12 "no client can predict it").
- **Reproducible from the log** (Round R12: "the order is reproducible from the log") for server-side debugging with the secret, never without it.
- **No engine patches; minimal dependencies;** testable as plain C++ (architecture.md "Testable cores": the seed mixer is a plain core).
- **Never logged:** `ServerSecret`, sub-seeds and stream keys never reach logs, traces, URLs or replication (Session F5; Round R19).

### Requirements
- R1 `ServerSecret` (128-bit) and `MatchNonce` (64-bit) from an OS CSPRNG at session open; test override in non-Shipping builds only.
- R2 Round `Seed` per F5 (purpose tag 0x01, redraw on `UsedSeeds` or Generator golden set), range [1, 2^31 − 1].
- R3 `PublicSubSeed(Domain, Attempt) = Mix32(...)` exactly as F5, Layout domain only, golden vectors.
- R4 `GetServerSubSeed(Domain, Attempt) = H64(...)` per F5 (tag 0x02), server-only, authority-asserted, never callable from the Generator.
- R5 A keyed-hash DRBG stream for hidden draws, with domain-separated stream tags (CP-9 `Deal`/`Replace`/`Take`), counter-based random access (CNB-R14), unbiased ranged integers (CP-8).
- R6 One integer-only public stream type for the Generator and decoration: `uint32` state outputs, multiply-shift with rejection, no `mod`, no floats (R-G3.1), stage streams from Mix32 (R-G3.2).
- R7 Golden-vector and statistical tests deterministic in CI (coding standards: no random seeds in tests).

## Decision

### 1. Two worlds of randomness, one module

All primitives live in `StealthFoundation` as plain C++ (no `UObject`, no `UWorld`):

| Primitive | Visibility | Header (ADR-0007 unique `Stealth` prefix) | Use |
|---|---|---|---|
| `SipHash24` (64-bit output, 128-bit key) | server | `Public/Server/Random/StealthSipHash.h` | F5 `H64`; secret streams |
| server seed service (in `FSessionCore`, ADR-0005) | server | `Public/Server/Random/StealthServerSeeds.h` | `ServerSecret`, `MatchNonce`, `Seed` draw, `GetServerSubSeed` |
| `FSecretStream` | server | `Public/Server/Random/StealthSecretStream.h` | every hidden draw |
| `Mix32` | public | `Public/View/Random/StealthMix32.h` | `PublicSubSeed`, Generator stage streams |
| `FIntStream` (PCG32) | public | `Public/View/Random/StealthIntStream.h` | Generator, client decoration |
| ranged-draw helpers | public | `Public/View/Random/StealthRangedDraw.h` | Lemire rejection, weighted pick, Fisher-Yates (templated on the stream type) |
| `StealthRandom::Tag("XXXXXXXX")` | public | `Public/View/Random/StealthRandomTag.h` | `constexpr uint64` stream tags packed little-endian from 8 ASCII bytes (multicharacter literals are `int` and implementation-defined, engine review 5) |

`Public/Server/` headers carry the ADR-0007 server-only guard, so presentation code cannot include a secret primitive. **Generator isolation (R-G2.2):** the Generator lives in `Source/StealthGameplay/Private/Features/CityGenerator/` (and its public provider header in `Public/Server/Features/CityGenerator/`); it legitimately includes server headers such as `ILayoutProvider`, so a blanket Server ban does not apply. The forbidden include is narrower: **no file under a `CityGenerator/` folder may include anything under `Public/Server/Random/`**. ADR-0007's include lint has no such rule today (rules (a)-(h)); this ADR requests an ADR-0007 amendment adding it as rule (i) (TD-ADR item 1). Until it lands, the runtime scope assert (§3) is the guard.

### 2. Entropy and the session secret

- `IEntropySource { virtual bool Fill(TArrayView<uint8>) = 0; }`. Production: `FOsEntropySource` — Windows `BCryptGenRandom(nullptr, …, BCRYPT_USE_SYSTEM_PREFERRED_RNG)` checked with `BCRYPT_SUCCESS`, included the engine's way (`Windows/WindowsHWrapper.h`, then `Windows/AllowWindowsPlatformTypes.h`, `<bcrypt.h>`, `Windows/HideWindowsPlatformTypes.h` [V wrappers in `Core/Public/Windows`; pattern in `WindowsConfigRules.cpp:43-46`]) and linked with a Win64-guarded `PublicSystemLibraries.Add("bcrypt.lib")` in `StealthFoundation.Build.cs` [V `ModuleRules.cs:1348`]. Core has no OS-CSPRNG wrapper for Windows [V grep, engine review]. Linux (ADR-0024): `getrandom`, as the engine's `FUnixPlatformGuid::CreateGuidFromRandomOnly` does [V `UnixPlatformGuid.h:32`, `.cpp:69`]. **Not `CreateGuid`**: Windows `CoCreateGuid` is not specified as a CSPRNG and Unix `CreateGuid` is UUIDv7 with a timestamp and only 74 random bits [V `WindowsPlatformMisc.cpp:1554-1560`, `UnixPlatformGuid.cpp:46-60`]. **Not `PlatformCrypto`** (enabled by default, not Beta, OpenSSL `RAND_bytes` [V `PlatformCrypto.uplugin:13-17`, `EncryptionContextOpenSSL.cpp:721-724`]): rejected because it adds an OpenSSL-backed plugin dependency to the Foundation module and lives in the engine's `Experimental` folder, for 24 bytes per session; it remains the drop-in alternative behind `IEntropySource`.
- `OpenSession` (ADR-0005) draws `ServerSecret` (16 bytes) and `MatchNonce` (8 bytes). A failing entropy call is fatal to `OpenSession`: the session does not open, the failure goes to the fault sink and a user-facing "could not host" message (Shipping has no logging and no `ensure` reporting [V `Build.h:200-211`]), and it is never silently replaced by a weak source.
- **Test injection:** `FDeterministicEntropySource` and a command-line `-StealthServerSecret=<32 hex>` exist only in non-Shipping builds (`#if !UE_BUILD_SHIPPING`, sentinel literal for the ADR-0004 AC-22 binary scan). Character Pool's `IEntropySource` test hook (CP-9) is this interface.

### 3. `H64` = SipHash-2-4, and the F5 derivations

- **SipHash-2-4** (Aumasson & Bernstein): 128-bit key, 64-bit output, a PRF designed for short inputs; ~40 lines of in-project C++ using only `uint64` add/rotate/xor; verified against the reference implementation's published test vectors (key `00..0f`, messages `00`, `00 01`, … up to 63 bytes).
- **Round seed (F5):** `Seed = (SipHash(ServerSecret, 0x01 ‖ MatchNonce ‖ RoundIndex ‖ Draw) mod (2^31 − 1)) + 1`, all fields fixed-width little-endian as F5 states; redraw `Draw + 1` while `Seed ∈ UsedSeeds ∪ GeneratorGoldenSeeds` (the golden list is a committed data file read by Session; Generator change request 2). The modulo bias of a 64-bit value reduced mod 2^31 − 1 is below 2^-32 and is accepted (F5 specifies `mod`).
- **Server sub-seed (F5):** `GetServerSubSeed(Domain, Attempt) = SipHash(ServerSecret, 0x02 ‖ MatchNonce ‖ RoundIndex ‖ Domain ‖ Attempt)` → `uint64`. **Misuse checks are plain `if` branches evaluated in every build configuration** (not `check`/`ensure`, which compile to nothing or report nothing in Shipping/Test [V `Build.h:210-211, 304-305, 332-333`; `AssertionMacros.h:469`]): no authority, not the game thread, or inside `FPublicOnlyScope` → return 0 and call the fault sink directly with the domain; in non-Shipping builds the same branch additionally fires `ensureMsgf` (the R-G2.2 "server assert") so tests and developers see it at once.
- **Generator guard (R-G2.2):** the Layout Service invokes providers inside an `FPublicOnlyScope` (RAII); its depth counter is **`thread_local`**, so the guard also covers a Generator stage run on a worker thread if ADR-0014 ever time-slices it off the game thread. `GetServerSubSeed` called inside the scope takes the misuse branch above. Combined with the include rule (§1), the Generator can neither name nor successfully call a secret API.
- **Byte packing:** every message is built byte by byte with shifts into a fixed buffer in F5's little-endian layout, never by `memcpy` of a struct (padding, endianness).
- **Public sub-seed (F5):** `PublicSubSeed(Domain, Attempt) = Mix32(Seed, Domain, Attempt)` with F5's exact reference mixer (all `uint32`, logical shifts, 0 → 1). The public API refuses every domain except `Layout` (returns 0 and raises an `ensure`, F5). Golden vectors from F5 are the `StealthGame.Session.SeedDerivationGolden` fixture.

### 4. Domain registry (append-only)

`enum class ESeedDomain : uint32` with **fixed explicit values**, never renumbered or reused; a retired domain keeps its number:

| Value | Domain | Kind | Consumer (GDD) | `Attempt` meaning |
|---|---|---|---|---|
| 1 | `Layout` | public | Layout Service / Generator (LSC R4, R-G2) | layout attempt |
| 2 | `ObjectiveSites` | secret | Layout Service selection | layout attempt |
| 3 | `AppearanceDeal` | secret | Character Pool (CP-9; `Attempt = 0`) | 0 |
| 4 | `PlayerPlacement` | secret | Layout Service start placement | layout attempt |
| 5 | `NpcRoutes` | secret | Crowd routes | layout attempt |
| 6 | `SpawnOrder` | secret | Round P6 spawn shuffle | layout attempt |
| 7 | `EmergencePick` | secret | LSC emergence | request sequence id |
| 8 | `RespawnPick` | secret | LSC `PickRespawnPawn` | request sequence id |
| 9 | `AmbientSchedule` | secret | LSC / Crowd ambient schedule | draw counter (F5 counter-as-Attempt shape, see below) |
| 10 | `StageSignal` | secret | Objectives OC-12 | round-global completion ordinal; Δ drawn in **integer milliseconds** (below) |
| 11 | `SeqOrder` | secret | Round R12 intake permutation | `ServerTickIndex` (low 32 bits) |
| 12 | `TieRank` | secret | Round R12 tie rank | `ServerTickIndex` (low 32 bits) |
| **13** | **`CrowdBehaviour`** | secret | Crowd CNB-R14 | layout attempt |
| **14** | **`BushSpotPick`** | secret | Concealment CON-R12 / F4 blind-strike spot pick (consumer ADR-0018, stream ADR-0017 §7, tag `CON_SPOT`) | layout attempt (amendment 2026-10-24) |

**Two consumption shapes, fixed per domain** (TD-ADR item 5):
- **Counter-as-Attempt** (F5's rule for domains 7-12, including `AmbientSchedule`): each draw event uses its own sub-seed `GetServerSubSeed(Domain, counter)` and an `FSecretStream(SubSeed, tag)` for the few draws inside that event. CNB-R14's "schedule draws use `AmbientSchedule` the same way" is read as "counter-based", satisfied by this shape; Session F5 is the contract owner, so its shape wins over a second reading.
- **Per-layout sub-seed + counter tuple** (`CrowdBehaviour`, CNB-R14; and `BushSpotPick`, ADR-0017 §7): one `GetServerSubSeed(Domain, LayoutAttempt)` per round and `Draw(tuple)` per decision — `(ordinal, decisionType, decisionIndex)` for the crowd, `(ServerTickIndex, AttackerCommandSeq, BushId)` for the spot pick, which then uses `UniformBelow` over the reach-filtered candidates.
- **OC-12 delays:** `Δ_ms = UniformInRange(FSecretStream(GetServerSubSeed(StageSignal, ordinal), Tag("OC_STAGE")), MinMs, MaxMs)` with `MinMs`/`MaxMs` = `RivalSignalDelayMinSec`/`MaxSec` × 1000 rounded to integers once at tuning freeze (ADR-0004); no float enters the draw.

Domain 14 (`BushSpotPick`) was appended by amendment (2026-10-24, ADR-0017); it replaces Concealment F4's `H(...) mod |Reachable|`. Domain 13 is **new in this ADR**: Crowd CNB-R14 consumes `GetServerSubSeed(CrowdBehaviour, LayoutAttempt)` but Session F5's table stops at 12 (GDD sync below). `ServerTickIndex` is 64-bit (ADR-0002); F5 encodes `Attempt` as 4 bytes, so the low 32 bits are used — a wrap needs 2^32 ticks (~2.3 years at 60 Hz), outside any session.

### 5. Secret streams: SipHash in counter mode (the keyed-hash DRBG)

```
FSecretStream(uint64 SubSeed, uint64 StreamTag)
  K = (SubSeed, KStreamConst)                          // second key word is one fixed public constant, never the tag
  Block(i)  = SipHash(K, 0x01 ‖ u64le(StreamTag) ‖ u64le(i))                 // mode 0x01: sequential stream
  NextU64() = Block(Counter++)
  Draw(i, j, k…) = SipHash(K, 0x02 ‖ u64le(StreamTag) ‖ u64le(i) ‖ u64le(j) ‖ …)  // mode 0x02: counter tuple (CNB-R14)
```

- **Domain separation by message, not by key** (TD-ADR item 3): SipHash makes no security claim for related keys, so the stream tag goes into the message with a mode byte, and the key's second word is one fixed public constant. Modes 0x01 and 0x02 cannot collide with each other, and messages of different lengths cannot collide (SipHash's final block encodes the length).
- **Why this satisfies CP-9:** a PRF in counter mode is a keyed-hash DRBG: outputs are independent of each other given the key, and observing any number of outputs does not reveal the key or any other output (PRF security), unlike an invertible PRNG (PCG32, LCG) whose state follows from a few outputs. The effective secret per stream is the 64-bit sub-seed (the second key word and the tag are public); recovering it from observed outputs needs ~2^64 SipHash evaluations, accepted for this threat model (a game cheat, not a cryptographic adversary). The root `ServerSecret` stays 128-bit. Upgrade path if ever needed: derive 128-bit stream keys with SipHash-2-4-128 (Alternative 5).
- **Stream tags** are fixed `uint64` constants per consumer, built with `StealthRandom::Tag("CPA_DEAL")` (8 ASCII bytes packed little-endian, `constexpr`) and documented in the consumer ADR (e.g. `Tag("CPA_DEAL")`, `Tag("CPA_REPL")`, `Tag("CPA_TAKE")`), so streams from one sub-seed never overlap (CP-9 domain separation).
- **Counter-based draws (CNB-R14):** `Draw(ordinal, decisionType, decisionIndex)` on a stream keyed by `GetServerSubSeed(CrowdBehaviour, LayoutAttempt)`; a pawn's k-th draw of a decision type is fixed regardless of other draws, kills or takeovers, because nothing advances a shared counter.
- **Per-tick permutations (Round R12):** `FSecretStream(GetServerSubSeed(SeqOrder, ServerTickIndex), Tag("RND_SEQ_"))` drives a Fisher-Yates over the canonically sorted non-empty mailboxes; `TieRank` likewise. Cost: one sub-seed (one SipHash) plus ≤ 10 draws per tick.
- **Unpredictability vs reproducibility:** with the `ServerSecret` (server memory only; test override in non-Shipping) every draw is reproducible from the logged `(Domain, Attempt, StreamTag, counter)`; without it no client can predict any draw.

### 6. Public integer stream for the Generator and decoration: PCG32 + Mix32

- **`FIntStream` = PCG32 (XSH-RR 64/32)**: 64-bit LCG state (multiplier `6364136223846793005`), 32-bit output by xorshift-high + random rotation (`rot` rotated with `(32u - rot) & 31u`, never unary minus on unsigned, MSVC C4146); seeded exactly as the reference `pcg32_srandom_r(initstate, initseq)` (`state = 0; inc = (initseq << 1) | 1; step; state += initstate; step`), because the reference outputs for `(42, 54)` — the golden fixture — depend on that routine. All arithmetic is `uint64`/`uint32` with defined wrap; shifts are logical; no float anywhere. R-G3.1's "FRandomStream-style state, `uint32` only" is read as "`uint32` outputs, integer-only state": PCG32's 64-bit integer state meets the intent (GDD sync below). Chosen over a bespoke Mix32 counter stream for its published reference vectors and statistical quality, and over `FRandomStream` (§ Alternatives).
- **Stage streams (R-G3.2):** `StageSeed(s, j) = Mix32(AttemptSeed, s, j)` seeds `FIntStream(StageSeed, s)`; changing a later stage never changes an earlier stage's draws; `j` is never folded into `Attempt`.
- **Ranged draws (R-G3.1, CP-8):** `UniformBelow(n)` = Lemire's multiply-shift with rejection: `m = uint64(x) * n; if (uint32(m) < n) { t = (2^32 − n) % n; while (uint32(m) < t) { x = next; m = uint64(x) * n; } } return m >> 32`. The one `%` computes the rejection threshold (written `(0u - n) % n` in `uint32`), not the result, so no modulo bias. Edge cases (TD-ADR item 9): `UniformBelow(0)` is refused (misuse branch, returns 0); `UniformInRange(Lo, Hi)` computes the span as `uint32(Hi) - uint32(Lo) + 1u`, and a span that wraps to 0 (the full 32-bit range) returns `NextU32()` directly; `Lo > Hi` is refused. `UniformInRange(lo, hi)`, integer-cumulative-weight picks and Fisher-Yates are templates usable with **both** `FIntStream` and `FSecretStream` (CP-8's unbiased shuffle uses the secret stream).
- **Canonical iteration (R-G3.3):** no unordered container (`TMap`/`TSet` iteration) ever feeds a draw; consumers sort by stable integer id first. A lint rule in ADR-0008's determinism suite flags `TMap`/`TSet` range-for inside Generator stage functions.
- **Floats** appear only in measurement, quantised to cm before comparison (R-G3.4); never in a draw.
- **Client decoration** (LSC R-3, Generator §decoration) uses `FIntStream(initstate = uint64(PublicSubSeed(Layout, LayoutAttempt)), initseq = LayoutHash)` (`LayoutHash` is 64-bit, ADR-0014), one stream per decoration pass, consumed in canonical cell order.
- **`Mix32` is `constexpr`** and the three F5 golden vectors are `static_assert`ed in its header, so every compiler checks them at build time (engine review 6).

### 7. Logging and evidence

- Never logged anywhere (output log, trace, Event Log, crash context, URL, replication): `ServerSecret`, `MatchNonce`, any server sub-seed, any stream key. Allowed: `Seed` (public), `LayoutAttempt`, domain/attempt/counter tuples, and digests (`DealDigest = SipHash(fixed public key, root sub-seed)`, CP-9) — a digest of a 64-bit secret through a PRF reveals nothing usable.
- The Round Event Log (server memory only, Round R19) may record domain/attempt/counter tuples so a server-side replay with the secret can reproduce an order.

### Architecture Diagram

```
 OS CSPRNG ──► IEntropySource ──► ServerSecret (128b), MatchNonce (64b)        [FSessionCore, server memory only]
                                       │
              ┌────────────────────────┼─────────────────────────────────────────────┐
              │ SipHash(ServerSecret, 0x01‖Nonce‖Round‖Draw)                           │ SipHash(ServerSecret, 0x02‖Nonce‖Round‖Domain‖Attempt)
              ▼                                                                       ▼
   Seed ∈ [1, 2^31−1]  (public: FRoundParams, URL)                 GetServerSubSeed(Domain 2..13, Attempt)  (secret, uint64)
              │                                                                       │
   Mix32(Seed, Layout=1, Attempt) = PublicSubSeed                    FSecretStream(SubSeed, StreamTag): SipHash counter mode
              │                                                       ├─ CPA Deal / Replace / Take (CP-8, CP-9, CP-12)
   Mix32(AttemptSeed, stage s, cand j) → FIntStream (PCG32)          ├─ Round SpawnOrder, SeqOrder, TieRank (R12, P6)
              ├─ Generator stages (R-G3), integer only                ├─ LSC ObjectiveSites, PlayerPlacement, EmergencePick, RespawnPick
              └─ client decoration (LSC R-3)                          ├─ Crowd CrowdBehaviour, NpcRoutes, AmbientSchedule (CNB-R14)
                                                                      └─ Objectives StageSignal (OC-12)
   (Generator code runs inside FPublicOnlyScope: any GetServerSubSeed call asserts)
```

### Key Interfaces

```cpp
// --- Public/View/Random (client-safe, public determinism) -----------------------
namespace StealthRandom {
    constexpr uint32 Mix32(uint32 Seed, uint32 Domain, uint32 Attempt);   // Session F5 reference mixer
}
struct FIntStream {                                     // PCG32 XSH-RR; integer only
    FIntStream(uint64 InitState, uint64 InitSeq);
    uint32 NextU32();
};
template<class TStream> uint32 UniformBelow(TStream&, uint32 N);            // Lemire multiply-shift with rejection
template<class TStream> int32  UniformInRange(TStream&, int32 Lo, int32 Hi);
template<class TStream> int32  WeightedPick(TStream&, TConstArrayView<uint32> CumulativeWeights);
template<class TStream, class T> void FisherYates(TStream&, TArrayView<T>);  // input must be canonically sorted

// --- Public/Server/Random (server-only; ADR-0007 guard) -------------------------
enum class ESeedDomain : uint32 { Layout = 1, ObjectiveSites = 2, AppearanceDeal = 3, PlayerPlacement = 4,
    NpcRoutes = 5, SpawnOrder = 6, EmergencePick = 7, RespawnPick = 8, AmbientSchedule = 9,
    StageSignal = 10, SeqOrder = 11, TieRank = 12, CrowdBehaviour = 13,
    BushSpotPick = 14 /* amendment 2026-10-24, ADR-0017 */ };   // append-only
class IEntropySource { public: virtual bool Fill(TArrayView<uint8> Out) = 0; };
uint64 SipHash24(const uint8 Key[16], TConstArrayView<uint8> Message);
struct FSecretStream {                                  // keyed-hash DRBG (SipHash counter mode)
    FSecretStream(uint64 SubSeed, uint64 StreamTag);
    uint64 NextU64();  uint32 NextU32();
    uint64 Draw(TConstArrayView<uint64> CounterTuple) const;              // counter-based random access
};
// On USessionService (ADR-0005), backed by FSessionCore:
int32  DrawRoundSeed(uint32 RoundIndex);                                   // F5, redraw on used/golden
uint32 PublicSubSeed(ESeedDomain Domain, uint32 Attempt) const;            // Layout only; ensure otherwise
uint64 GetServerSubSeed(ESeedDomain Domain, uint32 Attempt) const;         // server + game thread; asserts in FPublicOnlyScope
struct FPublicOnlyScope { FPublicOnlyScope(); ~FPublicOnlyScope(); };      // Layout Service wraps provider calls
```

### Implementation Guidelines
1. Gameplay and server code **must never** use `FMath::Rand`, `FMath::FRand`, `FMath::RandRange`, `FRandomStream`, `std::rand`, `<random>` engines or time-seeded randomness; every draw comes from `FSecretStream` (hidden) or `FIntStream` (public geometry/decoration). A CI grep enforces it with an allowlist for engine-facing cosmetic code that cannot affect simulation or presentation parity (none expected in MVP).
2. The public `Seed` and `PublicSubSeed` **must** drive only layout geometry, the socket set and client decoration; **every hidden decision must** draw from `GetServerSubSeed` via `FSecretStream`.
3. `ServerSecret`, `MatchNonce`, server sub-seeds and stream keys **must never** be logged, traced, replicated, put in a URL or exposed through Blueprint; only digests and `(Domain, Attempt, StreamTag, counter)` tuples may be logged.
4. New hidden purposes **must** append a new `ESeedDomain` value (never reuse or renumber) and a new stream tag, recorded in this ADR's table by amendment.
5. Ranged draws **must** use `UniformBelow`/`UniformInRange` (rejection, no `%` on the result, no floats); shuffles **must** be Fisher-Yates over canonically sorted arrays; no unordered container may feed a draw. **Sanctioned exception — continuous distributions on secret streams** (amendment 2026-10-24, ADR-0016 §9): a server-only consumer that needs a continuous value (a duration from an exponential, uniform or log-normal law) maps one 64-bit draw to `u = ((Bits >> 11) + 0.5) · 2^-53` (never 0 or 1) in `double` and applies a closed-form inverse CDF (normal quantile by AS241/PPND16); never on `FIntStream`, never on a client, never to pick among discrete options (those stay `UniformBelow`/`WeightedPick`). Golden vectors for `u` are exact; inverse-CDF goldens use a documented tolerance because `log`/`exp` may differ by platform, which is harmless because the values are server-only and never compared across machines. The result is converted once, at the point of use, to an integer tick or millisecond count (round to nearest, clamped to the tuning bounds) before it enters any gameplay state; the inverse-CDF golden tolerance is a relative 1e-12, recorded beside the vectors (TD confirmation, 2026-10-24).
6. Generator code **must** run inside `FPublicOnlyScope` and **must not** include `Public/Server/Random/`.
7. Golden vectors (SipHash, PCG32, Mix32, Seed derivation, Generator goldens) **must** be committed fixtures and run on every CI platform; changing a primitive **must** replace its vectors in the same change with a note in this ADR (F5: "An ADR may replace the mixer only by keeping the contract and replacing the golden vectors").
8. Tests **must** inject entropy through `IEntropySource`; no test may depend on OS entropy or wall-clock time.

## Alternatives Considered

### Alternative 1: `FRandomStream` everywhere (the prototype)
- **Description**: Seed engine `FRandomStream`s from the public seed (and from sub-seeds for hidden draws).
- **Pros**: Already used by the prototype; zero new code; deterministic within one build (10 PIE instances built the identical city).
- **Cons**: A 32-bit LCG whose state is recovered from about two observed outputs (each float fraction exposes ~23 bits), so any hidden stream on it is predictable almost at once; integer ranges derived from a float fraction [V `RandomStream.h:186-189`] (bias for large ranges; float path contradicts R-G3.1); its implementation is engine-owned and may change between versions, breaking golden seeds.
- **Rejection Reason**: Fails CP-9 (invertible), R-G3 (floats), and long-term golden stability.

### Alternative 2: HMAC-SHA256 or an OpenSSL DRBG through the `PlatformCrypto` plugin
- **Description**: Use a standard cryptographic DRBG (CTR-DRBG / HMAC-DRBG) from OpenSSL via the engine's PlatformCrypto plugin, or HMAC-SHA256 as `H64`.
- **Pros**: Standardised, high security margin; OS-grade entropy available from the same plugin [V `CreateRandomBytes`].
- **Cons**: The plugin lives in the engine's `Experimental` folder (though enabled by default and used by UnrealPak); adds an OpenSSL-backed plugin dependency to the Foundation module for a few hundred draws per second; HMAC-SHA256 is ~10x the cost of SipHash per block; more code to wrap and test.
- **Rejection Reason**: Disproportionate for the threat model; an extra plugin dependency in Foundation. Kept as an upgrade path behind `IEntropySource` / the stream interface.

### Alternative 3: Blake3 keyed mode
- **Description**: Use BLAKE3's keyed hash as both `H64` and the stream PRF.
- **Pros**: Fast, modern, 256-bit keys; Blake3 already ships in engine Core.
- **Cons**: The engine's `FBlake3` wrapper exposes no keyed mode [V `Blake3.h`]; using the underlying C library's keyed API would reach past the engine wrapper into an internal third-party module.
- **Rejection Reason**: No supported keyed API in the engine; SipHash is small enough to own outright.

### Alternative 4: One integer PRNG (PCG32) for both public and secret draws
- **Description**: Seed PCG32 from the secret sub-seed for hidden draws, as for public ones.
- **Pros**: One stream type; simplest.
- **Cons**: PCG32 is invertible from a few outputs (its state follows from observed values); CP-9 explicitly forbids it for `Replace`/`Take`, and the same weakness would let a cheater predict crowd behaviour from watching NPCs (a Pillar-1 identification aid: whoever deviates from the predicted behaviour is human).
- **Rejection Reason**: Violates CP-9 and the secrecy constraint.

### Alternative 5: 128-bit derived stream keys (SipHash-2-4-128)
- **Description**: Derive each secret stream's key as a 128-bit SipHash-128 output from `ServerSecret` instead of using the 64-bit sub-seed as the key word.
- **Pros**: Full 128-bit secrecy per stream.
- **Cons**: A second SipHash variant and a second "stream root" concept beside F5's 64-bit `ServerSubSeed`, which every consumer GDD already names.
- **Rejection Reason**: 64-bit per-stream secrecy is sufficient for the threat model; recorded as the upgrade path if a security review asks for more.

## Consequences

### Positive
- One small, owned set of primitives with published reference vectors; no engine RNG whose implementation can drift.
- Public and secret randomness cannot be confused: different types, different headers, include guards, a runtime scope assert and a CI grep.
- Secret streams are non-invertible and random-access, which gives CNB-R14's kill-independent counter draws and CP-9's domain-separated streams for free.
- Every hidden order is reproducible server-side from logged tuples plus the secret, which makes playtest bugs debuggable without weakening secrecy.

### Negative
- ~200 lines of in-project crypto-adjacent code (SipHash, PCG32, Lemire) to own and test.
- The prototype's generator must be rewritten to integer-only draws, and its objective/bush placement moved off the public seed (it was evidence only; ADR-0014 governs the replacement).
- 64-bit effective secrecy per stream (accepted; Alternative 5 is the upgrade).
- A new domain value (13) must be mirrored into Session F5's table (GDD sync).
- **The listen host is outside this threat model** (TD-ADR item 8): the host process holds `ServerSecret` in memory, so a modified host can predict or rewind every hidden draw. This is the same accepted MVP exposure as IVP R10 / architecture AQ-14 (trusted hosts only, host rotation); the guarantees here protect against the nine remote clients and end with the dedicated server (ADR-0024).

## Risks
- **R-1 Hand-written primitive bug** (SipHash, PCG32 or Lemire off-by-one). *Mitigation:* reference test vectors for SipHash and PCG32; Lemire tested exhaustively for small `n` and statistically (chi-square with a fixed seed, deterministic) for large `n`; F5 Mix32 golden vectors.
- **R-2 Cross-compiler drift** for public draws (signed shift, integer promotion of `uint16`/`uint8` operands). *Mitigation:* all arithmetic on `uint32`/`uint64` explicitly; `static_assert`s on types; golden vectors on every CI platform (MSVC now, clang with ADR-0024).
- **R-3 A consumer seeds a hidden decision from the public seed** (as the prototype does for objectives). *Mitigation:* `FPublicOnlyScope` assert, include lint, CI grep, and Pillar-1 review of every consumer ADR's stream table.
- **R-4 Secret leaks through a log or crash dump.** *Mitigation:* the secret types have no `ToString`/`UE_LOGF` formatter and are not `USTRUCT`/`UPROPERTY` (no reflection, no Blueprint, no replication); crash-context hooks never include `FSessionCore`.
- **R-5 OS entropy unavailable** (sandbox, exotic platform). *Mitigation:* `OpenSession` fails loudly rather than degrading; a platform port adds its `IEntropySource`.

## GDD Requirements Addressed

Provisional TRs covered (architecture.md Appendix A / ADR Audit): TR-session-004, TR-cpa-003, TR-citygen-002 (with ADR-0008), TR-crowd-005, TR-round-011, TR-oc-006 (with ADR-0020).

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| session-travel.md | F5: Seed derivation with purpose tags and redraw; `PublicSubSeed` via Mix32 (Layout only); `ServerSubSeed` via keyed `H64`; fixed-width LE encoding; golden vectors | §3 (SipHash-2-4 as `H64`, Mix32 verbatim, golden fixture), §4 |
| session-travel.md | Rule 14: public seed drives geometry only; every hidden decision from server-only sub-seeds; Session owns the secret and `GetServerSubSeed` | §1, §2, §3, Guideline 2 |
| character-pool-appearance.md | CP-8 unbiased Fisher-Yates with rejection-sampled integers | §6 `UniformBelow`, `FisherYates` on `FSecretStream` |
| character-pool-appearance.md | CP-9 server-private root `GetServerSubSeed(AppearanceDeal, 0)`; three domain-separated keyed-hash DRBG streams; not an invertible PRNG; `DealDigest` only in logs; `IEntropySource` injection compiled out of Shipping | §5 (SipHash counter mode, stream tags), §7, §2 |
| character-pool-appearance.md | CP-12 uniform replacement draw over vacant cards (stream `Replace`) | §5 + §6 `UniformBelow` |
| procedural-city-generator.md | R-G2: inputs only `PublicSubSeed(Layout, LayoutAttempt)`; server-assert if the Generator calls `GetServerSubSeed` | §3 `FPublicOnlyScope`, §1 include lint |
| procedural-city-generator.md | R-G3: one integer stream type, multiply-shift with rejection, integer weights, Fisher-Yates over sorted arrays, stage streams via Mix32, canonical iteration, floats only in quantised measurement | §6 |
| crowd-npc-behaviour.md | CNB-R14: counter-based draws `H(seed, ordinal, decisionType, decisionIndex)` from `GetServerSubSeed(CrowdBehaviour, LayoutAttempt)`, independent of kills, never `FMath::Rand` | §4 (domain 13), §5 `Draw(…)`, Guideline 1 |
| round-match-framework.md | R12: per-tick Fisher-Yates permutation from `GetServerSubSeed(SeqOrder, ServerTickIndex)`; `TieRank` per tick; reproducible from the log, unpredictable by clients | §4, §5 per-tick permutations, §7 |
| round-match-framework.md | R23(a)(b) / P6: spawn order a server-secret shuffle independent of player-ness | §4 `SpawnOrder`, §5 |
| objectives-chain.md | OC-12: fresh per-event delay on the server-secret `StageSignal` domain keyed by the round-global completion ordinal | §4 domain 10 (Attempt = ordinal), §5 |
| level-socket-contract.md | Emergence, respawn and ambient picks from secret domains; client decoration from `PublicSubSeed` + `LayoutHash` with an integer RNG | §4 domains 7-9, §6 decoration |

## Performance Implications
- **CPU**: SipHash-2-4 on ≤ 40-byte messages ≈ 20-40 ns; hidden draws are a few hundred per second (crowd decisions ~160/s at 40 NPCs × 4 Hz, permutations once per tick) → well under 0.05 ms/frame, inside the 0.5 ms queue/Roster line and the 1.5 ms crowd line. PCG32 ≈ 1-2 ns per draw; Generator draws are bounded by its own budget (R-G9).
- **Memory**: negligible (a stream is 16-24 bytes; the secret 24 bytes).
- **Load Time**: none.
- **Network**: none (only the public `Seed` is replicated, in `FRoundParams`, ADR-0005).

## Migration Plan
1. The prototype `AStealthCityGenerator` (`FRandomStream` with float draws, objectives and bushes placed from the public seed) is evidence of feasibility only: seeds 29755/8924/25482 produced the identical city on 3 and 10 PIE instances of one build on one machine (first-playable report), which shows determinism within one binary, not across compilers, and its objective placement is a secrecy defect this ADR forbids. It is replaced under ADR-0014, not migrated.
2. Order: SipHash + vectors → Mix32 + F5 vectors → PCG32 + vectors → Lemire/Fisher-Yates + tests → `FSecretStream` → Session `DrawRoundSeed`/`GetServerSubSeed` → consumers adopt in their own stories.

## Validation Criteria
- `StealthGame.Session.SeedDerivationGolden`: F5 Mix32 vectors; Seed derivation with a fixed test secret; redraw on a used seed and on a golden seed; `PublicSubSeed` refuses domain 2.
- `StealthGame.Random.SipHashVectors`, `StealthGame.Random.Pcg32Vectors`: reference vectors pass on every CI platform.
- `StealthGame.Random.UniformBelowUnbiased`: exhaustive for `n ≤ 64` over a full 2^16-output window of a fixed stream; chi-square at fixed seed for `n ∈ {3, 24, 1000, 2^31−1}`.
- `StealthGame.Random.SecretStreamIndependence`: counter-based draws for one pawn ordinal are identical with and without interleaved draws for other ordinals and decision types (CNB-R14).
- `StealthGame.Random.GeneratorCannotReadSecrets`: calling `GetServerSubSeed` inside `FPublicOnlyScope` raises the expected assert (expected-error hook).
- CI grep: no `FMath::Rand*`, `FRandomStream` or `<random>` in `StealthFoundation`/`StealthGameplay` outside the allowlist.
- Character Pool's G-DECK statistical gate and the Generator golden seeds (ADR-0008) run on these primitives.

## Related
- ADR-0005 (Session store holds the secret; `FRoundParams` carries `Seed`) — depends on.
- ADR-0002 (queue intake permutation and tie rank use `SeqOrder`/`TieRank`).
- ADR-0007 (server-only guard and include lint for `Public/Server/Random/`).
- ADR-0008 (golden-vector and statistical tests; Generator goldens).
- ADR-0012 (Character Pool stream tags), ADR-0014 (Generator stage streams and decoration), ADR-0016 (crowd counter draws), ADR-0019 (respawn pick), ADR-0020 (stage signal).
- **GDD / architecture sync (2026-10-24), change requests implied, none applied by this run:** (1) `session-travel.md` F5 Domain list stops at 12; this ADR registers `CrowdBehaviour = 13`, which `crowd-npc-behaviour.md` CNB-R14 already consumes; F5's "`H64` is any keyed 64-bit hash chosen in the ADR" → SipHash-2-4. (2) `procedural-city-generator.md` R-G3.1 "`uint32` only" → "`uint32` outputs, integer-only (PCG32 has 64-bit integer state)". (3) `crowd-npc-behaviour.md` CNB-R14 "schedule draws use `AmbientSchedule` the same way" → F5's counter-as-Attempt shape (§4). (4) `architecture.md` Foundation "Seeds & RNG" row uses `(Purpose, Index)` and "[U primitive]" → `(Domain, Attempt)`, primitive decided. (5) ADR-0007 amendment: include-lint rule (i) [applied as rule (j), 2026-10-24] "no `CityGenerator/` file includes `Public/Server/Random/`".
- Design: `design/gdd/session-travel.md` (F5, rule 14), `character-pool-appearance.md` (CP-8, CP-9, CP-12), `procedural-city-generator.md` (R-G2, R-G3), `crowd-npc-behaviour.md` (CNB-R14), `round-match-framework.md` (R12, R23, P6), `objectives-chain.md` (OC-12), `level-socket-contract.md` (decoration, emergence/respawn picks).

## Amendments (2026-10-24 sync)
Inter-ADR amendment pass; decision unchanged, Status unchanged (Proposed).
- §4 table, shapes, Key Interfaces: `BushSpotPick = 14` appended (per-layout sub-seed + counter-tuple shape, tag `CON_SPOT`) — requested by ADR-0017 (§7).
- Guideline 5: sanctioned unit-fraction mapping for continuous server-only draws, converted to integer ticks/ms at the point of use — requested by ADR-0016 (§9); wording confirmed by technical-director.
- `CrowdBehaviour = 13`: already registered here (the Session F5 GDD sync is the GDD owner's).
- Cross-reference only: this ADR's requested ADR-0007 include-lint rule landed as rule (j) (letter (i) was taken); Related note annotated.
