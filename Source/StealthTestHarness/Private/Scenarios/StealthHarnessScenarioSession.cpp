// Session-travel spike scenarios (THROWAWAY spike code, the ADR-0005 shape; see Spikes/Session/StealthSpikeSession.h):
//   "S3"   story 003: two seamless round travels (plus a carried-PlayerState travel and one hard travel) with
//          the four game-instance stores compared before/after and every participant's owner-only team view
//          acknowledged before the round's first tick;
//   "S4"   story 004: a mid-round rejoin that adopts the retained PlayerState, observed by raw-bunch capture
//          on the non-owner clients, then a supersede control;
//   "Soak" story 002: a lobby phase, one seamless round travel and an active round with moving players, under
//          packet timing (net profile measurement).
// Every process is launched by tools/ci/mp/run-mp.ps1 with -ServerUrlOptions naming the spike game mode and
// -IdentityKeys (distinct `?IdentityKey=` per client). Analysers: tools/ci/mp/analyze_s3.py, analyze_s4.py,
// analyze_net.py. Wall-clock time is only a ceiling; the analysers report, the spike reports judge.
#include "Scenarios/StealthHarnessRun.h"

#include "Session/StealthConnectionEventHook.h"
#include "Spikes/Session/StealthSpikeSession.h"
#include "StealthGameState.h"
#include "StealthTestHarnessModule.h"

#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Math/RandomStream.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace StealthHarness
{
	namespace SessionSpike
	{
		const TCHAR* const MatchMap = TEXT("/Game/StealthGame/Maps/L_Prototype");

		template <typename StoreType>
		static StoreType* GetStore(const UWorld* World)
		{
			const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			return GameInstance ? GameInstance->GetSubsystem<StoreType>() : nullptr;
		}

		/** Writes every store's content and hash under Tag; returns "s,r,o,t" hashes joined. */
		static FString SnapshotStores(FRun& Run, const UWorld* World, const FString& Tag)
		{
			const UStealthSpikeStoreBase* Stores[] = { GetStore<UStealthSpikeSessionStore>(World), GetStore<UStealthSpikeRosterStore>(World),
				GetStore<UStealthSpikeRoundStore>(World), GetStore<UStealthSpikeTuningStore>(World) };
			const TCHAR* Names[] = { TEXT("session"), TEXT("roster"), TEXT("round"), TEXT("tuning") };
			FString Fields;
			FString Hashes;
			for (int32 Index = 0; Index < 4; ++Index)
			{
				const uint32 Hash = Stores[Index] ? Stores[Index]->ContentHash() : 0;
				Fields += FString::Printf(TEXT(",\"%s\":{\"hash\":%u,\"content\":%s}"), Names[Index], Hash,
					*FRun::Q(Stores[Index] ? Stores[Index]->DescribeContent() : FString(TEXT("<missing>"))));
				Hashes += FString::Printf(TEXT("%s%u"), Index ? TEXT(",") : TEXT(""), Hash);
			}
			Run.Write(TEXT("stores"), FString::Printf(TEXT("\"tag\":%s%s,%s"), *FRun::Q(Tag), *Fields, *FRun::ServerTimeFields(World)));
			return Hashes;
		}

		/** Ids of the participants whose Session entry is Connected (they must acknowledge before the round ticks). */
		static TArray<int32> ConnectedParticipants(const UWorld* World)
		{
			TArray<int32> Ids;
			if (const UStealthSpikeSessionStore* Session = GetStore<UStealthSpikeSessionStore>(World))
			{
				for (const TPair<FString, UStealthSpikeSessionStore::FEntry>& Pair : Session->Entries)
				{
					if (Pair.Value.State == UStealthSpikeSessionStore::EConnection::Connected)
					{
						Ids.Add(Pair.Value.ParticipantId);
					}
				}
			}
			Ids.Sort();
			return Ids;
		}

		/** True when every id in Ids acknowledged the current epoch with the hash the server expects. */
		static bool AllAcked(const UWorld* World, const TArray<int32>& Ids, FString& OutDetail)
		{
			const UStealthSpikeSessionStore* Session = GetStore<UStealthSpikeSessionStore>(World);
			const UStealthSpikeRosterStore* Roster = GetStore<UStealthSpikeRosterStore>(World);
			if (!Session || !Roster || Ids.Num() == 0)
			{
				return false;
			}
			bool bAll = true;
			OutDetail.Reset();
			for (const int32 Id : Ids)
			{
				const uint32* Hash = Session->Acks.Find(Id);
				const uint32 Expected = Roster->BuildTeamView(Id).Hash;
				const bool bMatch = Hash && *Hash == Expected;
				bAll &= bMatch;
				OutDetail += FString::Printf(TEXT("%s{\"id\":%d,\"acked\":%d,\"match\":%d}"), OutDetail.IsEmpty() ? TEXT("") : TEXT(","), Id, (int32)(Hash != nullptr), (int32)bMatch);
			}
			return bAll;
		}

		/** Server: starts a round travel to the match map (epoch++, acks cleared, stores snapshotted first). Returns the participants expected back. */
		static TArray<int32> StartTravel(FRun& Run, UWorld* World, int32 Number, bool bSeamless)
		{
			const TArray<int32> Expected = ConnectedParticipants(World);
			if (UStealthSpikeRosterStore* Roster = GetStore<UStealthSpikeRosterStore>(World))
			{
				++Roster->RoundEpoch; // the round-scope reset ordering: the epoch moves before the travel starts
			}
			if (UStealthSpikeSessionStore* Session = GetStore<UStealthSpikeSessionStore>(World))
			{
				Session->Acks.Reset();
				// A hard travel closes every connection and the clients log in again: Session counts that as travel.
				Session->bHardTravelInProgress = !bSeamless;
			}
			const FString Hashes = SnapshotStores(Run, World, FString::Printf(TEXT("pre%d"), Number));
			AGameModeBase* GameMode = World->GetAuthGameMode();
			if (GameMode)
			{
				GameMode->bUseSeamlessTravel = bSeamless;
			}
			const FString Url = FString(MatchMap) + StealthSpikeSession::GameOption();
			Run.Write(TEXT("serverTravel"), FString::Printf(TEXT("\"n\":%d,\"seamless\":%d,\"url\":%s,\"preHashes\":%s,%s"),
				Number, (int32)bSeamless, *FRun::Q(Url), *FRun::Q(Hashes), *FRun::ServerTimeFields(World)));
			World->ServerTravel(Url, /*bAbsolute*/ false);
			return Expected;
		}

		/** True when World is a fully begun match world different from Previous (the travel arrived). */
		static bool Arrived(const UWorld* World, const UWorld* Previous)
		{
			return World && World != Previous && World->HasBegunPlay() && World->GetAuthGameMode() != nullptr
				&& World->GetOutermost()->GetName().EndsWith(TEXT("L_Prototype"));
		}

		/** Client: acknowledges a new team view through the owner-only RPC; writes the player array when it changes. */
		struct FClientTracker
		{
			int32 AckedEpoch = -1;
			uint32 AckedHash = 0;
			FString LastArray;

			void Tick(FRun& Run, UWorld* World)
			{
				APlayerController* PC = World->GetFirstPlayerController();
				AStealthSpikePlayerController* Spike = Cast<AStealthSpikePlayerController>(PC);
				const AStealthSpikePlayerState* PS = PC ? Cast<AStealthSpikePlayerState>(PC->PlayerState) : nullptr;
				if (Spike && PS && PS->TeamView.ParticipantId >= 0 && (PS->TeamView.RoundEpoch != AckedEpoch || PS->TeamView.Hash != AckedHash))
				{
					AckedEpoch = PS->TeamView.RoundEpoch;
					AckedHash = PS->TeamView.Hash;
					Spike->ServerSpikeAck(AckedEpoch, AckedHash);
					Run.Write(TEXT("ackSent"), FString::Printf(TEXT("\"epoch\":%d,\"hash\":%u,\"id\":%d"), AckedEpoch, AckedHash, PS->TeamView.ParticipantId));
				}
				if (const AGameStateBase* GameState = World->GetGameState())
				{
					FString Array;
					for (const APlayerState* Member : GameState->PlayerArray)
					{
						Array += FString::Printf(TEXT("%s%s"), Array.IsEmpty() ? TEXT("") : TEXT(","), *FRun::Q(FRun::Guid(World, Member)));
					}
					if (Array != LastArray)
					{
						LastArray = Array;
						Run.Write(TEXT("playerArray"), FString::Printf(TEXT("\"n\":%d,\"guids\":[%s]"), GameState->PlayerArray.Num(), *Array));
					}
				}
			}
		};

		static FString OwnKey(const FRun& Run)
		{
			FString Key;
			return FParse::Value(FCommandLine::Get(), TEXT("-IdentityKey="), Key) ? Key : FStealthConnectionEventHook::IdentityKeyForSlot(Run.GetClientIndex());
		}
	}

	// ======================================================================================================
	// S3: stores and team views survive seamless travels (and a carried-PlayerState travel and a hard travel).
	// ======================================================================================================
	class FS3Scenario final : public IScenario
	{
		enum class EStep : uint8 { Lobby, Travelling, AwaitAcks, Active, Done };

		struct FTravelPlan
		{
			bool bSeamless;
			bool bDropC2Before; // E21: C2 is disconnected before this travel and rejoins after it
		};

	public:
		virtual void OnAllJoined(FRun& Run, UWorld* World) override
		{
			StepAt = World->GetTimeSeconds();
			SessionSpike::SnapshotStores(Run, World, TEXT("lobby"));
		}

		virtual bool TickServer(FRun& Run, UWorld* World, double ScenarioSeconds) override
		{
			static const FTravelPlan Plan[] = { { true, false }, { true, false }, { true, true }, { false, false } };
			const double Now = World->GetTimeSeconds();
			if (ScenarioSeconds > 600.0)
			{
				Run.Write(TEXT("event"), TEXT("\"ev\":\"ceiling\""));
				return true;
			}
			switch (Step)
			{
			case EStep::Lobby:
			case EStep::Active:
				if (World != StepWorld)
				{
					StepWorld = World;
					StepAt = Now;
				}
				if (Now - StepAt >= (Step == EStep::Lobby ? 5.0 : 8.0))
				{
					if (Travel >= UE_ARRAY_COUNT(Plan))
					{
						Step = EStep::Done;
						return true;
					}
					if (Plan[Travel].bDropC2Before && !bDroppedC2)
					{
						// E21 case: C2 leaves mid-round (server-side close); its PlayerState is retained and carried.
						if (APlayerController* C2 = FRun::FindPlayer(World, TEXT("C2")); C2 && C2->GetNetConnection())
						{
							Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"dropC2\",%s"), *FRun::ServerTimeFields(World)));
							C2->GetNetConnection()->Close();
						}
						bDroppedC2 = true;
						StepAt = Now - 6.0; // travel 2 s after the drop
						return false;
					}
					PreWorld = World;
					Expected = SessionSpike::StartTravel(Run, World, Travel + 1, Plan[Travel].bSeamless);
					Step = EStep::Travelling;
					StepAt = Now;
				}
				return false;
			case EStep::Travelling:
				if (SessionSpike::Arrived(World, PreWorld))
				{
					Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"arrived\",\"n\":%d,%s"), Travel + 1, *FRun::ServerTimeFields(World)));
					Step = EStep::AwaitAcks;
					StepWorld = World;
					AwaitSince = FPlatformTime::Seconds();
				}
				return false;
			case EStep::AwaitAcks:
			{
				FString Detail;
				const bool bAll = SessionSpike::AllAcked(World, Expected, Detail);
				const bool bCeiling = FPlatformTime::Seconds() - AwaitSince > 120.0;
				if (!bAll && !bCeiling)
				{
					return false;
				}
				if (UStealthSpikeSessionStore* Session = SessionSpike::GetStore<UStealthSpikeSessionStore>(World))
				{
					Session->bHardTravelInProgress = false;
				}
				// The round's first tick: the stores must equal their pre-travel content, every view acknowledged.
				const FString Hashes = SessionSpike::SnapshotStores(Run, World, FString::Printf(TEXT("post%d"), Travel + 1));
				Run.Write(TEXT("roundTick"), FString::Printf(TEXT("\"n\":%d,\"allAcked\":%d,\"ceiling\":%d,\"expected\":%d,\"acks\":[%s],\"postHashes\":%s,%s"),
					Travel + 1, (int32)bAll, (int32)bCeiling, Expected.Num(), *Detail, *FRun::Q(Hashes), *FRun::ServerTimeFields(World)));
				if (UStealthSpikeRoundStore* Round = SessionSpike::GetStore<UStealthSpikeRoundStore>(World))
				{
					++Round->RoundsPlayed;
				}
				if (AStealthGameMode* GameMode = World->GetAuthGameMode<AStealthGameMode>())
				{
					GameMode->StartRound();
				}
				++Travel;
				Step = EStep::Active;
				StepAt = Now;
				if (bDroppedC2 && !bLoggedRejoinWait)
				{
					bLoggedRejoinWait = true;
					StepAt = Now - 8.0 + 40.0; // hold the round 40 s so C2 can rejoin into it (adoption after travel)
				}
				return false;
			}
			default:
				return true;
			}
		}

		virtual bool ClientToleratesDisconnect(FRun& Run) const override
		{
			return StealthSpikeSession::ClientExpectsReconnect() || (Run.GetClientIndex() == 2 && !bC2Rejoined);
		}

		virtual void OnClientConnected(FRun& Run, UWorld* World) override
		{
			StealthSpikeSession::ClientExpectsReconnect() = false;
			if (bC2Dropped)
			{
				bC2Rejoined = true;
			}
		}

		virtual void TickClientOffline(FRun& Run, UWorld* World, double) override
		{
			if (Run.GetClientIndex() != 2 || StealthSpikeSession::ClientExpectsReconnect() || bC2Rejoined)
			{
				return;
			}
			const double Now = FPlatformTime::Seconds();
			if (!bC2Dropped)
			{
				bC2Dropped = true;
				C2DroppedAt = Now;
				Run.Write(TEXT("event"), TEXT("\"ev\":\"c2Dropped\""));
			}
			// Rejoin once, 30 s after the drop (the server has travelled meanwhile; the carried PlayerState is adopted).
			if (!bC2RejoinIssued && Now - C2DroppedAt >= 30.0 && World)
			{
				bC2RejoinIssued = true;
				FString Error;
				const bool bOk = FStealthConnectionEventHook::Rejoin(FStealthConnectionEventHook::ContextOf(World), SessionSpike::OwnKey(Run), TEXT("C2"), FString(), &Error);
				Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"c2Rejoin\",\"ok\":%d,\"error\":%s"), (int32)bOk, *FRun::Q(Error)));
			}
		}

		virtual void TickClient(FRun& Run, UWorld* World, double) override
		{
			Tracker.Tick(Run, World);
		}

	private:
		EStep Step = EStep::Lobby;
		int32 Travel = 0;
		double StepAt = 0.0;
		double AwaitSince = 0.0;
		const UWorld* StepWorld = nullptr;
		const UWorld* PreWorld = nullptr;
		bool bDroppedC2 = false;
		bool bLoggedRejoinWait = false;
		TArray<int32> Expected;
		// Client side.
		SessionSpike::FClientTracker Tracker;
		bool bC2Dropped = false;
		bool bC2Rejoined = false;
		bool bC2RejoinIssued = false;
		double C2DroppedAt = 0.0;
	};

	static TUniquePtr<IScenario> MakeS3Scenario() { return MakeUnique<FS3Scenario>(); }
	static FScenarioRegistrar GRegisterS3(TEXT("S3"), &MakeS3Scenario);

	// ======================================================================================================
	// S4: mid-round rejoin adopts the retained PlayerState; non-owner clients see no change; supersede control.
	// ======================================================================================================
	class FS4Scenario final : public IScenario
	{
	public:
		virtual void OnAllJoined(FRun& Run, UWorld* World) override
		{
			if (AStealthGameMode* GameMode = World->GetAuthGameMode<AStealthGameMode>())
			{
				GameMode->StartRound();
			}
			Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"startRound\",%s"), *FRun::ServerTimeFields(World)));
		}

		virtual bool TickServer(FRun& Run, UWorld* World, double ScenarioSeconds) override
		{
			// The clients drive the rejoin (C1 leaves at ~12 s, back by ~45 s: an uncooked map load is ~15 s) and the
			// supersede control (C3 at ~55 s, connected ~75 s); the host ends at 100 s.
			return ScenarioSeconds >= 100.0;
		}

		virtual bool ClientToleratesDisconnect(FRun& Run) const override
		{
			return (Run.GetClientIndex() == 1 && bC1Dropped && !bC1Rejoined) || (Run.GetClientIndex() == 3 && bSupersedeIssued && !bC3Reconnected);
		}

		virtual void OnClientConnected(FRun& Run, UWorld* World) override
		{
			if (bSupersedeIssued && !bC3Reconnected)
			{
				bC3Reconnected = true;
				Run.Write(TEXT("event"), TEXT("\"ev\":\"c3SupersedeConnected\""));
			}
			if (bC1Dropped && !bC1Rejoined)
			{
				bC1Rejoined = true;
				Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"c1RejoinConnected\",\"sinceIssue\":%.6f"), FPlatformTime::Seconds() - RejoinIssuedAt));
			}
		}

		virtual void TickClient(FRun& Run, UWorld* World, double ClientSeconds) override
		{
			Tracker.Tick(Run, World);
			const AStealthGameState* GS = World->GetGameState<AStealthGameState>();
			if (RoundSeenAt < 0.0 && GS && GS->RoundPhase == EStealthRoundPhase::InProgress)
			{
				RoundSeenAt = FPlatformTime::Seconds();
				Run.Write(TEXT("event"), TEXT("\"ev\":\"roundSeen\""));
			}
			if (RoundSeenAt < 0.0)
			{
				return;
			}
			const double SinceRound = FPlatformTime::Seconds() - RoundSeenAt;
			if (Run.GetClientIndex() == 1 && !bC1Dropped && SinceRound >= 12.0)
			{
				// Mid-round leave through the engine's own disconnect path (socket close, quit).
				bC1Dropped = true;
				FStealthConnectionEventHook Hook(nullptr);
				FString Error;
				const bool bOk = Hook.Drop(FStealthConnectionEventHook::ContextOf(World), EStealthDropMode::ClientQuit, &Error);
				DroppedAt = FPlatformTime::Seconds();
				Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"c1Drop\",\"ok\":%d,\"error\":%s"), (int32)bOk, *FRun::Q(Error)));
			}
			if (Run.GetClientIndex() == 3 && !bSupersedeIssued && SinceRound >= 55.0)
			{
				// Control: C3 joins with C1's key while C1 is live: the server must supersede exactly once.
				bSupersedeIssued = true;
				FString Error;
				const bool bOk = FStealthConnectionEventHook::Rejoin(FStealthConnectionEventHook::ContextOf(World), FStealthConnectionEventHook::IdentityKeyForSlot(1), TEXT("C3"), FString(), &Error);
				Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"c3SupersedeIssued\",\"ok\":%d,\"error\":%s"), (int32)bOk, *FRun::Q(Error)));
			}
		}

		virtual void TickClientOffline(FRun& Run, UWorld* World, double) override
		{
			if (Run.GetClientIndex() == 1 && bC1Dropped && !bRejoinIssued && World && World->HasBegunPlay() && FPlatformTime::Seconds() - DroppedAt >= 5.0)
			{
				bRejoinIssued = true;
				RejoinIssuedAt = FPlatformTime::Seconds();
				FString Error;
				const bool bOk = FStealthConnectionEventHook::Rejoin(FStealthConnectionEventHook::ContextOf(World), SessionSpike::OwnKey(Run), TEXT("C1"), FString(), &Error);
				Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"c1RejoinIssued\",\"ok\":%d,\"error\":%s"), (int32)bOk, *FRun::Q(Error)));
			}
		}

	private:
		SessionSpike::FClientTracker Tracker;
		double RoundSeenAt = -1.0;
		double DroppedAt = 0.0;
		double RejoinIssuedAt = 0.0;
		bool bC1Dropped = false;
		bool bRejoinIssued = false;
		bool bC1Rejoined = false;
		bool bSupersedeIssued = false;
		bool bC3Reconnected = false;
	};

	static TUniquePtr<IScenario> MakeS4Scenario() { return MakeUnique<FS4Scenario>(); }
	static FScenarioRegistrar GRegisterS4(TEXT("S4"), &MakeS4Scenario);

	// ======================================================================================================
	// Soak: lobby, one seamless round travel, active round with moving players (net-emulation measurement).
	// ======================================================================================================
	class FSoakScenario final : public IScenario
	{
		enum class EStep : uint8 { Lobby, Travelling, AwaitAcks, Active };

	public:
		virtual void OnAllJoined(FRun& Run, UWorld* World) override
		{
			LobbySeconds = FRun::IntParam(TEXT("LobbySeconds"), 300);
			TotalSeconds = FRun::IntParam(TEXT("Duration"), 1800);
			Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"lobby\",\"lobbySeconds\":%d,\"totalSeconds\":%d,%s"), LobbySeconds, TotalSeconds, *FRun::ServerTimeFields(World)));
		}

		virtual bool TickServer(FRun& Run, UWorld* World, double ScenarioSeconds) override
		{
			if (ScenarioSeconds >= TotalSeconds)
			{
				Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"end\",%s"), *FRun::ServerTimeFields(World)));
				return true;
			}
			switch (Step)
			{
			case EStep::Lobby:
				if (ScenarioSeconds >= LobbySeconds)
				{
					PreWorld = World;
					Expected = SessionSpike::StartTravel(Run, World, 1, /*bSeamless*/ true);
					Step = EStep::Travelling;
				}
				break;
			case EStep::Travelling:
				if (SessionSpike::Arrived(World, PreWorld))
				{
					Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"arrived\",\"n\":1,%s"), *FRun::ServerTimeFields(World)));
					Step = EStep::AwaitAcks;
					AwaitSince = FPlatformTime::Seconds();
				}
				break;
			case EStep::AwaitAcks:
			{
				FString Detail;
				const bool bAll = SessionSpike::AllAcked(World, Expected, Detail);
				if (bAll || FPlatformTime::Seconds() - AwaitSince > 120.0)
				{
					Run.Write(TEXT("roundTick"), FString::Printf(TEXT("\"n\":1,\"allAcked\":%d,\"acks\":[%s],%s"), (int32)bAll, *Detail, *FRun::ServerTimeFields(World)));
					if (AStealthGameMode* GameMode = World->GetAuthGameMode<AStealthGameMode>())
					{
						GameMode->StartRound();
					}
					Step = EStep::Active;
				}
				break;
			}
			default:
				break;
			}
			return false;
		}

		virtual void TickClient(FRun& Run, UWorld* World, double ClientSeconds) override
		{
			Tracker.Tick(Run, World);
			const AStealthGameState* GS = World->GetGameState<AStealthGameState>();
			APlayerController* PC = World->GetFirstPlayerController();
			APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			if (!Pawn || !GS || GS->RoundPhase != EStealthRoundPhase::InProgress)
			{
				return; // idle in the lobby: the minimum packet rate
			}
			// Active round: a deterministic random walk (new heading every 3 s), like a player moving through the crowd.
			const int32 Leg = FMath::FloorToInt(ClientSeconds / 3.0);
			if (Leg != LastLeg)
			{
				LastLeg = Leg;
				FRandomStream Stream(Run.GetClientIndex() * 7919 + Leg);
				Heading = FVector(Stream.FRandRange(-1.f, 1.f), Stream.FRandRange(-1.f, 1.f), 0.f).GetSafeNormal();
				if (Heading.IsNearlyZero())
				{
					Heading = FVector::ForwardVector;
				}
				PC->SetControlRotation(Heading.Rotation());
			}
			Pawn->AddMovementInput(Heading, 1.f);
		}

	private:
		EStep Step = EStep::Lobby;
		int32 LobbySeconds = 300;
		int32 TotalSeconds = 1800;
		double AwaitSince = 0.0;
		const UWorld* PreWorld = nullptr;
		TArray<int32> Expected;
		SessionSpike::FClientTracker Tracker;
		int32 LastLeg = -1;
		FVector Heading = FVector::ForwardVector;
	};

	static TUniquePtr<IScenario> MakeSoakScenario() { return MakeUnique<FSoakScenario>(); }
	static FScenarioRegistrar GRegisterSoak(TEXT("Soak"), &MakeSoakScenario);
}
