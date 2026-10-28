// NPC-Kill Penalty story nkp-018, AC1 / AC-NKP-19 (PEN-R15; HUD-R3, HUD-R7; ADR-0008 T2), the widget-state half:
// StealthGame.Penalty.PIE.TallyWidgetParity.NpcKillAndInformantKillDumpEqualAtSameTally. Listen server + 2 clients on
// L_Prototype with a crowd, 2v1. The solo player S kills a pair player P1 (Assassinated, no tally); P1's informant takes over
// a crowd pawn after InformantRespawnDelaySec; S then kills that informant body (WrongTarget: S's tally 1, no strike). The
// other pair player P2 kills an NPC (WrongTarget: P2's tally 1, one strike). One second after each plate the dumped tally
// widget state of each owner machine (FPenaltyTallyView::Dump: text, filled squares, triangle, frozen flag, colours, as the
// overlay paints it) is captured: the two dumps at tally 1 are byte-equal, and the HUD element lists' Tally entries are equal.
// The screenshots of each value 0-4 and the frozen tally are the run-and-observe evidence (nkp-19-*.png).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "StealthGameMode.h"
#include "StealthHUD.h"
#include "StealthSettings.h"
#include "Tests/PIE/Penalty/PenaltyPresentationPieSupport.h"

namespace TallyWidgetParityPieData
{
	constexpr double SettleSec = 0.6;
	constexpr double BodySettleSec = 1.0;
	/** The dump is taken this long after the result was struck (the plate has shown and the tally ticked). */
	constexpr double AfterStrikeSec = 1.5;
}

NETWORK_TEST_CLASS(TallyWidgetParity, "StealthGame.Penalty.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TWeakObjectPtr<AStealthPlayerController> Solo;
	TWeakObjectPtr<AStealthPlayerController> Pair[2];
	FParticipantId P1;
	TWeakObjectPtr<APawn> FirstBody;
	TWeakObjectPtr<APawn> InformantBody;
	uint32 Seq = 0x55000000u;
	double Mark = 0.0;
	FString InformantKillDump;
	FString NpcKillDump;
	FString InformantKillElement;
	FString NpcKillElement;

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

	double Now() const { return StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()); }

	void Wait(const TCHAR* Label, double Seconds)
	{
		Network
			.Then(*FString::Printf(TEXT("%s: mark"), Label), [this]() { Mark = Now(); })
			.Until(Label, [this, Seconds]() { return Now() >= Mark + Seconds; });
	}

	static AStealthPawn* PawnOf(const TWeakObjectPtr<AStealthPlayerController>& Controller)
	{
		return Controller.IsValid() ? Cast<AStealthPawn>(Controller->GetPawn()) : nullptr;
	}

	/** The owner machine's tally dump and its HUD element list's Tally entry. */
	static void Capture(const TWeakObjectPtr<AStealthPlayerController>& Player, FString& OutDump, FString& OutElement)
	{
		const AStealthPlayerController* Machine = StealthAssassinationPie::OwnerMachineController(Player.Get());
		if (Machine == nullptr)
		{
			return;
		}
		OutDump = Machine->BuildPenaltyHudFrame(StealthPenaltyPresentationPie::MachineNow(Machine)).Tally.Dump();
		TArray<FString> Elements;
		AStealthHUD::CollectOwnerElements(*Machine, Elements);
		for (const FString& Element : Elements)
		{
			if (Element.StartsWith(TEXT("Tally:")))
			{
				OutElement = Element;
			}
		}
	}

	TEST_METHOD(NpcKillAndInformantKillDumpEqualAtSameTally)
	{
		using namespace StealthAssassinationPie;
		using namespace StealthPenaltyPresentationPie;
		Network
			.Until(TEXT("the round is Active, the crowd is up and the players split 2v1"), [this]()
			{
				AStealthPlayerController* S = nullptr;
				AStealthPlayerController* P[2] = { nullptr, nullptr };
				if (!RoundActive() || !SplitTeams(S, P) || StealthPenaltyPie::Penalty() == nullptr)
				{
					return false;
				}
				Solo = S;
				Pair[0] = P[0];
				Pair[1] = P[1];
				P1 = StealthPenaltyPie::ParticipantOf(P[0]);
				return PawnOf(Solo) != nullptr && OwnerMachineController(S) != nullptr && OwnerMachineController(P[1]) != nullptr;
			})
			.Then(TEXT("place the players"), [this]()
			{
				MoveClearOfBushes(PawnOf(Solo));
				const FVector Right = FVector::CrossProduct(FVector::UpVector, PawnOf(Solo)->GetActorForwardVector().GetSafeNormal2D());
				PawnOf(Pair[1])->TeleportTo(PawnOf(Solo)->GetActorLocation() + Right * 1500.0, PawnOf(Pair[1])->GetActorRotation());
				MoveClearOfBushes(PawnOf(Pair[1]));
			});
		Wait(TEXT("settle"), TallyWidgetParityPieData::SettleSec);
		Network
			.Then(TEXT("S kills P1 (Assassinated: no tally)"), [this]()
			{
				FirstBody = PawnOf(Pair[0]);
				PlaceFacing(FirstBody.Get(), PawnOf(Solo), 150.0);
				StrikeAt(PawnOf(Solo), FirstBody.Get(), Seq);
			})
			.Until(TEXT("P1's informant is embodied in a crowd pawn"), [this]()
			{
				const UPenaltyService* Penalty = StealthPenaltyPie::Penalty();
				APawn* Body = StealthPenaltyPie::BodyOf(P1);
				if (Penalty == nullptr || Penalty->GetLifeState(P1) != EInformantLifeState::Embodied || Body == nullptr || Body == FirstBody.Get())
				{
					return false;
				}
				InformantBody = Body;
				return true;
			});
		Wait(TEXT("the body settles"), TallyWidgetParityPieData::BodySettleSec);
		Network.Then(TEXT("S kills P1's informant body (tally 1); P2 kills an NPC (tally 1)"), [this]()
		{
			MoveClearOfBushes(InformantBody.Get());
			PlaceFacing(InformantBody.Get(), PawnOf(Solo), 150.0);
			StrikeAt(PawnOf(Solo), InformantBody.Get(), Seq);
			NpcKill(PawnOf(Pair[1]), Seq);
		});
		Wait(TEXT("both plates show"), TallyWidgetParityPieData::AfterStrikeSec);
		Network.Then(TEXT("capture and compare the two owners' tally widget state"), [this]()
		{
			Capture(Solo, InformantKillDump, InformantKillElement);
			Capture(Pair[1], NpcKillDump, NpcKillElement);
			TestRunner->AddInfo(FString::Printf(TEXT("NKP018 parity: informant kill (S) %s [%s]"), *InformantKillDump, *InformantKillElement));
			TestRunner->AddInfo(FString::Printf(TEXT("NKP018 parity: NPC kill (P2) %s [%s]"), *NpcKillDump, *NpcKillElement));
			ASSERT_THAT(IsTrue(InformantKillDump.Contains(TEXT("text=Wrong targets: 1|")), TEXT("the informant kill shows tally 1 on S's machine")));
			ASSERT_THAT(IsTrue(NpcKillDump.Contains(TEXT("text=Wrong targets: 1|")), TEXT("the NPC kill shows tally 1 on P2's machine")));
			ASSERT_THAT(IsTrue(InformantKillDump == NpcKillDump, TEXT("the dumped tally widget state (text, squares, triangle, colours) is byte-equal for the two victim kinds")));
			ASSERT_THAT(IsTrue(InformantKillElement == NpcKillElement && !InformantKillElement.IsEmpty(), TEXT("the HUD element lists' tally entries are equal")));
		});
	}
};

#endif
