// Concealment story con-033 AC3 (production/epics/concealment-bushes/story-033-blind-strike-parity-and-kill-timing.md; GDD
// AC-CON-25 PIE form; the two-client form of story con-018): StealthGame.Concealment.PIE.NoImmunityWhileEntering.<method>.
// Listen host + 2 clients on L_Prototype, NpcCount 0. The entrant V (a remote client) starts Hide(Enter); at +0.3 s of its
// BushEnter (before the commit at +0.5 s) the attacker S strikes it by TargetPawn (an aimed PawnRef strike).
//   TargetPawnStrikeAtPlusPointThreeShowsTheFallEverywhere: V dies as a visible pawn: the server never commits it (no
//     Concealed, no cull record beyond Visible), its death write carries no bDiedConcealed, and on every machine V's body
//     is drawn on its contact frame (+- one frame) with its Death_Fall starting there: the pending swap is cancelled. The
//     latch set at the swap time on clients (they cannot know the outcome before the death write) clears at the death
//     write's arrival, before the contact.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Server/Visibility/StealthConcealmentCull.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/OwnPresentationPieSupport.h"
#include "Tests/PIE/Assassination/PresentationPieSupport.h"
#include "Tests/PIE/Concealment/ConcealmentPieSupport.h"

NETWORK_TEST_CLASS(NoImmunityWhileEntering, "StealthGame.Concealment.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousTimeout;
	inline static int32 PreviousNpcCount = -1;

	AStealthPlayerController* Entrant = nullptr;
	AStealthPlayerController* Attacker = nullptr;
	TWeakObjectPtr<AStealthPawn> Victim;
	FBushRuntime Bush;
	StealthOwnPresentationPie::FFrameTracker Frames;
	double Start = -1.0;
	double Contact = -1.0;
	bool bStruck = false;
	bool bEverConcealed = false;
	bool bEverCulled = false;
	bool bDrawnAtContact[3] = {};
	bool bSampledContact[3] = {};
	double HiddenFor[3] = {};
	double LastNow[3] = {};
	uint32 Seq = 0x33300000u;
	double Mark = 0.0;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousTimeout = Timeout->GetString();
			Timeout->Set(StealthAssassinationPie::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
		UStealthSettings* Settings = GetMutableDefault<UStealthSettings>();
		PreviousNpcCount = Settings->NpcCount;
		Settings->NpcCount = 0;
	}

	AFTER_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName); Timeout && !PreviousTimeout.IsEmpty())
		{
			Timeout->Set(*PreviousTimeout, ECVF_SetByCode);
		}
		if (PreviousNpcCount >= 0)
		{
			GetMutableDefault<UStealthSettings>()->NpcCount = PreviousNpcCount;
		}
	}

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>()
			.AsListenServer()
			.WithClients(2)
			.WithGameMode(AStealthGameMode::StaticClass())
			.Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	TEST_METHOD(TargetPawnStrikeAtPlusPointThreeShowsTheFallEverywhere)
	{
		using namespace StealthAssassinationPie;
		using namespace StealthConcealmentPie;
		Network
			.Until(TEXT("three players in Active, bushes built, Concealment bound"), []()
			{
				const UConcealmentService* Service = UConcealmentService::Get(ServerWorld());
				const UBushRuntimeSubsystem* Built = UBushRuntimeSubsystem::Get(ServerWorld());
				return RoundActive() && ServerPlayers().Num() == 3 && Service != nullptr && Service->IsBound() && Built != nullptr && Built->IsBuilt();
			})
			.Then(TEXT("V (a remote client) at the +X entry; its opponent S 80 cm behind it"), [this]()
			{
				AStealthPlayerController* Solo = nullptr;
				AStealthPlayerController* Pair[2] = { nullptr, nullptr };
				ASSERT_THAT(IsTrue(SplitTeams(Solo, Pair), TEXT("a 2v1 split")));
				Entrant = !Pair[0]->IsLocalController() ? Pair[0] : Pair[1];
				Attacker = Solo;
				Victim = Cast<AStealthPawn>(Entrant->GetPawn());
				Bush = UBushRuntimeSubsystem::Get(ServerWorld())->GetBushes()[0];
				PlacePlayerAt(Entrant, Bush.Anchor + FVector(130.0, 0.0, 0.0), FRotator(0.0, 180.0, 0.0));
				PlacePlayerAt(Attacker, Bush.Anchor + FVector(210.0, 0.0, 0.0), FRotator(0.0, 180.0, 0.0));
				for (AStealthPlayerController* Player : ServerPlayers())
				{
					if (Player != Entrant && Player != Attacker)
					{
						PlacePlayerAt(Player, Bush.Anchor + FVector(500.0, 400.0, 0.0), FRotator(0.0, -140.0, 0.0));
					}
				}
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("settle"), [this]() { Frames.Sample(); return ServerNow(ServerWorld()) >= Mark + 1.0; })
			.Then(TEXT("V enters"), [this]()
			{
				const uint16 BushId = Bush.Id;
				AsOwner(Victim.Get(), Seq, [BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("strike at +0.3 s of BushEnter, then sample every machine through the fall"), [this]()
			{
				Frames.Sample();
				const AStealthPawn* V = Victim.Get();
				const double Now = ServerNow(ServerWorld());
				if (V == nullptr)
				{
					return true;
				}
				if (Start < 0.0 && V->GetActionState().Pose == EActionPose::BushEnter)
				{
					Start = V->GetActionState().StartServerTime;
				}
				if (Start > 0.0 && !bStruck && Now >= Start + 0.3)
				{
					bStruck = true;
					StrikeAt(Cast<AStealthPawn>(Attacker->GetPawn()), Victim.Get(), Seq);
				}
				const AStealthPawn* S = Cast<AStealthPawn>(Attacker->GetPawn());
				if (bStruck && Contact < 0.0 && S != nullptr && S->GetActionState().Pose == EActionPose::Strike)
				{
					Contact = S->GetActionState().ContactServerTime;
				}
				bEverConcealed |= StateOf(V) == EConcealState::Concealed;
				const UStealthConcealmentCull* Cull = UStealthConcealmentCull::Get(ServerWorld());
				bEverCulled |= Cull != nullptr && Cull->GetState(V) != EConcealCullState::Visible;
				const TArray<UWorld*> All = Worlds();
				for (int32 Machine = 0; Machine < All.Num() && Machine < 3; ++Machine)
				{
					const AStealthPawn* Copy = Cast<AStealthPawn>(CopyByGuid(All[Machine], V));
					const double MachineNow = ServerNow(All[Machine]);
					if (Start > 0.0 && MachineNow >= Start && !IsBodyDrawn(Copy) && LastNow[Machine] > 0.0)
					{
						HiddenFor[Machine] += MachineNow - LastNow[Machine];
					}
					LastNow[Machine] = MachineNow;
					if (Contact > 0.0 && !bSampledContact[Machine] && MachineNow >= Contact)
					{
						bSampledContact[Machine] = true;
						bDrawnAtContact[Machine] = IsBodyDrawn(Copy);
					}
				}
				return (Contact > 0.0 && Now >= Contact + 1.0) || Now >= Mark + 5.0;
			})
			.Then(TEXT("assert"), [this]()
			{
				const AStealthPawn* V = Victim.Get();
				ASSERT_THAT(IsTrue(Start > 0.0 && bStruck && Contact > 0.0, TEXT("V's BushEnter began and S struck it at +0.3 s")));
				ASSERT_THAT(IsTrue(V != nullptr && V->HasDeathWrite() && !V->GetDeathState().bDiedConcealed, TEXT("V died as a visible pawn (no bDiedConcealed)")));
				ASSERT_THAT(IsFalse(bEverConcealed, TEXT("the server never committed V")));
				ASSERT_THAT(IsFalse(bEverCulled, TEXT("no cull record left Visible for V")));
				const TArray<UWorld*> All = StealthAssassinationPie::Worlds();
				for (int32 Machine = 0; Machine < All.Num() && Machine < 3; ++Machine)
				{
					const AStealthPawn* Copy = Cast<AStealthPawn>(StealthConcealmentPie::CopyByGuid(All[Machine], V));
					const FEncounterPresentationLogRow* Fall = Copy != nullptr ? StealthPresentationPie::FirstRow(All[Machine], Copy, EEncounterPresentationEvent::ClipStart, EEncounterClip::DeathFall) : nullptr;
					const double Frame = Frames.FrameOf(Machine);
					TestRunner->AddInfo(FString::Printf(TEXT("CON25 machine %d: drawn at contact %d, Death_Fall at %.3f (contact %.3f, start + %.3f), body hidden %.3f s in total"),
						Machine, bDrawnAtContact[Machine] ? 1 : 0, Fall != nullptr ? Fall->ServerNow : -1.0, Contact, Contact - Start, HiddenFor[Machine]));
					ASSERT_THAT(IsTrue(bSampledContact[Machine] && bDrawnAtContact[Machine], *FString::Printf(TEXT("machine %d: V's body is drawn at its contact (the swap was cancelled)"), Machine)));
					ASSERT_THAT(IsTrue(Fall != nullptr && StealthOwnPresentationPie::OnFrameOf(Fall->ServerNow, Contact, Frame), *FString::Printf(TEXT("machine %d: the fall is visible from the contact frame"), Machine)));
				}
			});
	}
};

#endif
