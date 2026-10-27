// Assassination story asn-024 AC3 / AC-ASN-40 (ASN-R8, IIF UI-4; ADR-0008 T2; listen server + 2 clients on L_Prototype,
// NpcCount 0): StealthGame.Assassination.PIE.NoBushTargetingUi.<method>. A remote client's player A is the attacker; an
// enemy player H hides in one bush (occupied), another bush is empty. A's camera (its control rotation, the camera ray) is turned
// onto nothing (the baseline), onto the occupied bush and onto the empty bush, holding each for a dwell, then A strikes the
// occupied bush from outside reach (a whiff) and, once its plate has gone, the three aims are repeated. On A's machine the
// HUD's element list (AStealthHUD::CollectOwnerElements, the widget list DrawHUD draws from) is identical for the occupied
// and the empty bush and equal to an unaimed baseline before and after the strike: no Strike hint, prompt, highlight or
// reticle exists for a bush. Evidence asn-40-{occupied,empty}.png (the pixel half) is the run-and-observe step's.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "StealthBush.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/OwnPresentationPieSupport.h"

namespace NoBushTargetingUiPieData
{
	/** A stands this far from the occupied bush (outside strike reach: the strike is a whiff). */
	constexpr double StandOffCm = 700.0;
	/** Each aim is held this long before its element list is taken. */
	constexpr double DwellSec = 0.4;
	/** Past the whiff's plate (3.85 s from contact) and the lockout. */
	constexpr double PlateGoneSec = 4.6;
	constexpr double SettleSec = 0.8;

	enum EAim : int32 { Baseline, Occupied, Empty, BaselineAgain, AimCount };
	const TCHAR* AimName(int32 Aim)
	{
		const TCHAR* const Names[AimCount] = { TEXT("baseline"), TEXT("occupied bush"), TEXT("empty bush"), TEXT("baseline again") };
		return Names[Aim];
	}
}

NETWORK_TEST_CLASS(NoBushTargetingUi, "StealthGame.Assassination.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TWeakObjectPtr<AStealthPlayerController> Attacker;
	TWeakObjectPtr<AStealthPlayerController> Hider;
	TWeakObjectPtr<AStealthBush> OccupiedBush;
	TWeakObjectPtr<AStealthBush> EmptyBush;
	int32 MachineA = INDEX_NONE;
	/** [before / after the strike][aim]. */
	TArray<FString> Lists[2][NoBushTargetingUiPieData::AimCount];
	bool bConcealed = false;
	uint32 Seq = 0x24300000u;
	double Mark = 0.0;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(StealthAssassinationPie::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
		UStealthSettings* Settings = GetMutableDefault<UStealthSettings>();
		PreviousNpcCount = Settings->NpcCount;
		Settings->NpcCount = 0;
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

	/** Turns A's camera on its own machine (the local controller owns the control rotation) toward Target, or up at the sky. */
	void Aim(const AActor* Target)
	{
		AStealthPlayerController* Local = StealthOwnPresentationPie::LocalOf(MachineA);
		const APawn* Pawn = Local != nullptr ? Local->GetPawn() : nullptr;
		if (Pawn == nullptr)
		{
			return;
		}
		const FRotator Rotation = Target != nullptr ? (Target->GetActorLocation() - Pawn->GetActorLocation()).Rotation() : FRotator(60.0, Pawn->GetActorRotation().Yaw + 90.0, 0.0);
		Local->SetControlRotation(Rotation);
	}

	void Capture(int32 Phase, int32 AimIndex)
	{
		using namespace StealthAssassinationPie;
		const AActor* Target = AimIndex == NoBushTargetingUiPieData::Occupied ? static_cast<const AActor*>(OccupiedBush.Get())
			: (AimIndex == NoBushTargetingUiPieData::Empty ? static_cast<const AActor*>(EmptyBush.Get()) : nullptr);
		Network
			.Then(*FString::Printf(TEXT("%d aim at %s"), Phase, NoBushTargetingUiPieData::AimName(AimIndex)), [this, Target]() { Aim(Target); Mark = ServerNow(ServerWorld()); })
			.Until(*FString::Printf(TEXT("%d dwell on %s"), Phase, NoBushTargetingUiPieData::AimName(AimIndex)), [this, Target]()
			{
				Aim(Target); // held every frame, as a player's aim would be
				return ServerNow(ServerWorld()) >= Mark + NoBushTargetingUiPieData::DwellSec;
			})
			.Then(*FString::Printf(TEXT("%d take the element list"), Phase), [this, Phase, AimIndex]() { Lists[Phase][AimIndex] = StealthOwnPresentationPie::ElementsOf(MachineA); });
	}

	void CaptureAll(int32 Phase)
	{
		for (int32 AimIndex = 0; AimIndex < NoBushTargetingUiPieData::AimCount; ++AimIndex)
		{
			Capture(Phase, AimIndex);
		}
	}

	TEST_METHOD(OccupiedAndEmptyBushShowTheUnaimedWidgetListBeforeAndAfterAStrike)
	{
		using namespace StealthAssassinationPie;
		using namespace NoBushTargetingUiPieData;
		Network
			.Until(TEXT("the round is Active with both players up"), [this]()
			{
				AStealthPlayerController* S = nullptr;
				AStealthPlayerController* P[2] = { nullptr, nullptr };
				if (!RoundActive() || !SplitTeams(S, P))
				{
					return false;
				}
				// A remote client strikes (its own machine is not the server's); the hider is on the other team.
				Attacker = !S->IsLocalController() ? S : (P[0]->IsLocalController() ? P[1] : P[0]);
				Hider = Attacker.Get() == S ? P[0] : S;
				MachineA = StealthPresentationPie::MachineOf(Attacker.Get());
				return MachineA != INDEX_NONE && StealthOwnPresentationPie::LocalOf(MachineA) != nullptr;
			})
			.Then(TEXT("H hides in a bush, A stands off it; pick an empty bush"), [this]()
			{
				TArray<AStealthBush*> Bushes;
				for (TActorIterator<AStealthBush> It(ServerWorld()); It; ++It)
				{
					Bushes.Add(*It);
				}
				if (Bushes.Num() < 2)
				{
					return;
				}
				OccupiedBush = Bushes[0];
				EmptyBush = Bushes[1];
				Hider->GetPawn()->TeleportTo(OccupiedBush->GetActorLocation() + FVector(0.0, 0.0, 100.0), Hider->GetPawn()->GetActorRotation());
				const FVector Toward = (EmptyBush->GetActorLocation() - OccupiedBush->GetActorLocation()).GetSafeNormal2D();
				Attacker->GetPawn()->TeleportTo(OccupiedBush->GetActorLocation() + Toward * StandOffCm + FVector(0.0, 0.0, 100.0), Attacker->GetPawn()->GetActorRotation());
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("H is concealed (or 5 s)"), [this]()
			{
				const AStealthCharacter* Hidden = Hider.IsValid() ? Cast<AStealthCharacter>(Hider->GetPawn()) : nullptr;
				bConcealed = OccupiedBush.IsValid() && Hidden != nullptr && Hidden->IsConcealed();
				return (bConcealed && ServerNow(ServerWorld()) >= Mark + SettleSec) || ServerNow(ServerWorld()) >= Mark + 5.0;
			})
			.Then(TEXT("the occupied bush is occupied"), [this]() { ASSERT_THAT(IsTrue(bConcealed && EmptyBush.IsValid(), TEXT("H is concealed in the occupied bush; a second bush exists"))); });
		CaptureAll(0);
		Network
			.Then(TEXT("A strikes the occupied bush from outside reach"), [this]()
			{
				FAttackPayload Payload;
				Payload.WorldPoint = OccupiedBush->GetActorLocation();
				AsOwner(Cast<AStealthPawn>(Attacker->GetPawn()), Seq, [&Payload](UPawnCommandComponent& Command) { Command.Attack(Payload); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("the whiff's plate has come and gone"), [this]() { return ServerNow(ServerWorld()) >= Mark + PlateGoneSec; });
		CaptureAll(1);

		Network.Then(TEXT("assert"), [this]()
		{
			for (int32 Phase = 0; Phase < 2; ++Phase)
			{
				for (int32 AimIndex = 0; AimIndex < AimCount; ++AimIndex)
				{
					TestRunner->AddInfo(FString::Printf(TEXT("ASN40 %s strike, %s: [%s]"), Phase == 0 ? TEXT("before") : TEXT("after"), AimName(AimIndex),
						*FString::Join(Lists[Phase][AimIndex], TEXT(", "))));
				}
				ASSERT_THAT(IsTrue(Lists[Phase][Baseline].Num() > 0, TEXT("the baseline element list was taken")));
				ASSERT_THAT(IsTrue(Lists[Phase][Occupied] == Lists[Phase][Empty], *FString::Printf(TEXT("%s the strike: occupied and empty bush show the same widget list"), Phase == 0 ? TEXT("before") : TEXT("after"))));
				// A message expiring between two captures changes the list for an unrelated reason: one of the two baselines matches.
				ASSERT_THAT(IsTrue(Lists[Phase][Occupied] == Lists[Phase][Baseline] || Lists[Phase][Occupied] == Lists[Phase][BaselineAgain],
					*FString::Printf(TEXT("%s the strike: aiming at a bush adds or changes no element against the unaimed baseline"), Phase == 0 ? TEXT("before") : TEXT("after"))));
				for (const FString& Element : Lists[Phase][Occupied])
				{
					ASSERT_THAT(IsFalse(Element.Contains(TEXT("Hint")) || Element.Contains(TEXT("Prompt")) || Element.Contains(TEXT("Reticle")) || Element.Contains(TEXT("Highlight")),
						*FString::Printf(TEXT("no hint, prompt, reticle or highlight element (%s)"), *Element)));
				}
			}
		});
	}
};

#endif
