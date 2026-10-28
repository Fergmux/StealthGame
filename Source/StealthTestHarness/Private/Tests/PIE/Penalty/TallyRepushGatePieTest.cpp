// NPC-Kill Penalty story nkp-017, AC3 / AC-NKP-46 (PEN-R3, PEN-R15, Penalty CR-12 b; HUD-R7; ADR-0008 T2):
// StealthGame.Penalty.PIE.TallyRepushGate.OlderRepushFirstNeverMovesTheTallyBeforeThePlate. Listen server + 2 clients on
// L_Prototype with a crowd, 2v1; the attacker is the pair player on a client machine. Kill 1 (an NPC) shows tally 1 at its
// plate. Then, in one server frame: a rejoin re-push (UPenaltyService::OnViewRebound, the call the Roster's OnViewRebound
// makes after a rejoin; Session's PIE rejoin is not built, so the test makes the call) writes {tally 1, LastResultSeq of kill 1} to the attacker's C-TeamView
// owner part, and kill 2 (an NPC) resolves with its FStrikeResult held 500 ms on the attacker's machine (the lateness seam).
// The re-push therefore arrives first, with an OLDER LastResultSeq than kill 2's result (equal to the last shown plate's).
// Sampled every frame on the attacker's machine from kill 2 until one second after its plate: the HUD tally
// (GetShownWrongTargetTally) and the client-local display cache (UStealthOwnerTallyCache) never change before the plate's
// show time, never decrease within the round, and equal kill 2's TallyAfter from the show time on. Positive control: the
// re-push really reached the owner part on that machine before the plate ({1, kill 1's seq}).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "Engine/LocalPlayer.h"
#include "HAL/IConsoleManager.h"
#include "StealthGameMode.h"
#include "StealthGameState.h"
#include "View/StealthOwnerTallyCache.h"
#include "StealthSettings.h"
#include "Tests/PIE/Penalty/PenaltyPresentationPieSupport.h"
#include "View/StealthRosterTeamView.h"
#include "View/StealthRoundPublicState.h"

namespace TallyRepushGatePieData
{
	constexpr double SettleSec = 0.6;
	/** The FStrikeResult RPC delay (AC-NKP-46). */
	constexpr double ResultDelaySec = 0.5;
	/** The sampling continues this long after kill 2's plate. */
	constexpr double AfterPlateSec = 1.0;
}

NETWORK_TEST_CLASS(TallyRepushGate, "StealthGame.Penalty.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TWeakObjectPtr<AStealthPlayerController> Attacker;
	TWeakObjectPtr<AStealthPlayerController> Machine;
	FParticipantId AttackerId;
	uint32 Seq = 0x53000000u;
	double Mark = 0.0;
	int32 LogFrom = 0;
	uint32 Kill1Seq = 0;
	StealthPenaltyPresentationPie::FMachineFrame Frames;

	struct FSample
	{
		double Time = 0.0;
		int32 Shown = 0;
		int32 Cached = 0;
	};
	TArray<FSample> Series;
	bool bRepushSeen = false;
	double RepushSeenAt = -1.0;
	double PlateAt = -1.0;
	int32 PlateTally = -1;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(StealthAssassinationPie::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
		UStealthSettings* Settings = GetMutableDefault<UStealthSettings>();
		PreviousNpcCount = Settings->NpcCount;
		Settings->NpcCount = StealthPenaltyPie::CrowdNpcCount;
		StealthPenaltyHarnessSeams::SetCrowdlessTeamStartRespawn(false);
	}

	AFTER_ALL()
	{
		StealthOwnerPresentationTestSeams::SetReplyLatenessSec(0.0);
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName); Timeout && !PreviousNetworkTimeout.IsEmpty())
		{
			Timeout->Set(*PreviousNetworkTimeout, ECVF_SetByCode);
		}
		if (PreviousNpcCount >= 0)
		{
			GetMutableDefault<UStealthSettings>()->NpcCount = PreviousNpcCount;
		}
	}

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(2).WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	AFTER_EACH()
	{
		StealthOwnerPresentationTestSeams::SetReplyLatenessSec(0.0);
	}

	double Now() const { return StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()); }

	void Wait(const TCHAR* Label, double Seconds)
	{
		Network
			.Then(*FString::Printf(TEXT("%s: mark"), Label), [this]() { Mark = Now(); })
			.Until(Label, [this, Seconds]() { return Now() >= Mark + Seconds; });
	}

	AStealthPawn* AttackerPawn() const { return Attacker.IsValid() ? Cast<AStealthPawn>(Attacker->GetPawn()) : nullptr; }

	int32 CachedTally() const
	{
		const ULocalPlayer* Local = Machine.IsValid() ? Machine->GetLocalPlayer() : nullptr;
		const UStealthOwnerTallyCache* Cache = Local != nullptr ? Local->GetSubsystem<UStealthOwnerTallyCache>() : nullptr;
		const AStealthGameState* State = Machine.IsValid() ? Machine->GetWorld()->GetGameState<AStealthGameState>() : nullptr;
		return Cache != nullptr && State != nullptr ? Cache->GetTally(State->GetRoundPublicState().RoundIndex) : -1;
	}

	TEST_METHOD(OlderRepushFirstNeverMovesTheTallyBeforeThePlate)
	{
		using namespace StealthAssassinationPie;
		using namespace StealthPenaltyPresentationPie;
		Network
			.Until(TEXT("the round is Active, 2v1, a pair player is a client"), [this]()
			{
				AStealthPlayerController* S = nullptr;
				AStealthPlayerController* P[2] = { nullptr, nullptr };
				if (!RoundActive() || !SplitTeams(S, P) || StealthPenaltyPie::Penalty() == nullptr)
				{
					return false;
				}
				AStealthPlayerController* Client = ClientPairPlayer(P);
				if (Client == nullptr || OwnerMachineController(Client) == nullptr)
				{
					return false;
				}
				Attacker = Client;
				Machine = OwnerMachineController(Client);
				AttackerId = StealthPenaltyPie::ParticipantOf(Client);
				return AttackerPawn() != nullptr;
			})
			.Then(TEXT("the attacker stands clear of the bushes"), [this]() { MoveClearOfBushes(AttackerPawn()); });
		Wait(TEXT("settle"), TallyRepushGatePieData::SettleSec);
		Network.Then(TEXT("kill 1: an NPC"), [this]() { NpcKill(AttackerPawn(), Seq); });
		Wait(TEXT("kill 1's plate and the lockout"), StrikeStepSec);
		Network
			.Then(TEXT("the reclaim re-push (older seq), then kill 2 with its result held 500 ms"), [this]()
			{
				ASSERT_THAT(IsTrue(Machine->GetShownWrongTargetTally() == 1, TEXT("fixture: kill 1's plate showed tally 1")));
				// Kill 1's ClientCommandSeq.Seq (StrikeAt stamps ++Seq): the re-push's LastResultSeq.
				Kill1Seq = Seq;
				StealthPenaltyPie::Penalty()->OnViewRebound(AttackerId);
				StealthOwnerPresentationTestSeams::SetReplyLatenessSec(TallyRepushGatePieData::ResultDelaySec);
				LogFrom = Machine->GetOwnPresentationLog().Num();
				NpcKill(AttackerPawn(), Seq);
			})
			.Until(TEXT("sample every frame until one second after kill 2's plate"), [this]()
			{
				const AStealthPlayerController* Local = Machine.Get();
				if (Local == nullptr)
				{
					return false;
				}
				Frames.Sample(Local);
				const double LocalNow = MachineNow(Local);
				Series.Add({ LocalNow, Local->GetShownWrongTargetTally(), CachedTally() });
				const FTeamViewOwnerPart& Part = Local->GetTeamView()->GetView().Owner;
				if (!bRepushSeen && Part.LastResultSeq == Kill1Seq && Part.WrongTargetTally == 1 && PlateAt < 0.0)
				{
					bRepushSeen = true;
					RepushSeenAt = LocalNow;
				}
				const TArray<FOwnPresentationRecord> Plates = RecordsOf(Local, EOwnPresentationEvent::ResultPlate, LogFrom);
				if (PlateAt < 0.0 && Plates.Num() > 0)
				{
					PlateAt = Plates[0].ServerNow;
					const TArray<FOwnPresentationRecord> Ticks = RecordsOf(Local, EOwnPresentationEvent::TallyTick, LogFrom);
					PlateTally = Ticks.Num() > 0 ? Ticks.Last().Value : -1;
				}
				return PlateAt >= 0.0 && LocalNow >= PlateAt + TallyRepushGatePieData::AfterPlateSec;
			})
			.Then(TEXT("assert the gate"), [this]()
			{
				int32 ChangedBeforePlate = 0;
				int32 Decreases = 0;
				int32 WrongAfterPlate = 0;
				int32 CacheChangedBeforePlate = 0;
				int32 CacheWrongAfterPlate = 0;
				for (int32 Index = 0; Index < Series.Num(); ++Index)
				{
					const FSample& S = Series[Index];
					const bool bBefore = S.Time + 0.001 < PlateAt;
					ChangedBeforePlate += bBefore && S.Shown != 1 ? 1 : 0;
					CacheChangedBeforePlate += bBefore && S.Cached != 1 ? 1 : 0;
					WrongAfterPlate += !bBefore && S.Shown != PlateTally ? 1 : 0;
					CacheWrongAfterPlate += !bBefore && S.Cached != PlateTally ? 1 : 0;
					Decreases += Index > 0 && S.Shown < Series[Index - 1].Shown ? 1 : 0;
					Decreases += Index > 0 && S.Cached < Series[Index - 1].Cached ? 1 : 0;
				}
				TestRunner->AddInfo(FString::Printf(TEXT("NKP017 repush: kill 1 seq %u; re-push {1, %u} seen at %.3f (%d); kill 2 plate at %.3f with tally %d; %d frames; before the plate changed %d (cache %d), after the plate wrong %d (cache %d), decreases %d"),
					Kill1Seq, Kill1Seq, RepushSeenAt, bRepushSeen ? 1 : 0, PlateAt, PlateTally, Series.Num(), ChangedBeforePlate, CacheChangedBeforePlate, WrongAfterPlate,
					CacheWrongAfterPlate, Decreases));
				ASSERT_THAT(IsTrue(Kill1Seq != 0, TEXT("fixture: kill 1 set the tally's LastResultSeq")));
				ASSERT_THAT(IsTrue(bRepushSeen && RepushSeenAt + 0.001 < PlateAt, TEXT("positive control: the older re-push reached the owner part before kill 2's plate")));
				ASSERT_THAT(IsTrue(PlateTally == 2, TEXT("kill 2's plate carries TallyAfter 2")));
				ASSERT_THAT(IsTrue(ChangedBeforePlate == 0 && CacheChangedBeforePlate == 0, TEXT("the HUD tally and the display cache never change before the plate's show time")));
				ASSERT_THAT(IsTrue(Decreases == 0, TEXT("the HUD tally and the display cache never decrease within the round")));
				ASSERT_THAT(IsTrue(WrongAfterPlate == 0 && CacheWrongAfterPlate == 0, TEXT("from the plate's show time on, both equal TallyAfter")));
			});
	}
};

#endif
