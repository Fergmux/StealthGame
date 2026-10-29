// Concealment story con-032 AC1 (production/epics/concealment-bushes/story-032-ownership-change-while-concealed.md; GDD
// AC-CON-16, CON-R11; ADR-0017 section 3, ADR-0003 cull): StealthGame.Concealment.PIE.DisconnectRelease.<method>. Listen
// host + 2 clients on L_Prototype, NpcCount 0. The hider H (a remote client) conceals; its ally A and the opponent E watch.
//   ConcealedDisconnecterStaysHiddenThroughGraceThenExitsAsAnNpc: H's participant drops (the Session-level disconnect: Round
//     reads Disconnected, the Roster projection Disconnected; a composition-root test seam, so the pawn is kept as the R17
//     hold requires and the client machine can still be observed). Through the Round R17 grace the pawn stays
//     Concealed and issues no Hide (its FActionState is unchanged, its cull Culled, E holds no actor). At grace expiry the
//     Round releases the pawn to the crowd (Roster ReleaseToCrowd): it is a concealed NPC, culled for its former ally A at
//     the next cull boundary (A's actor closes; A never draws the body meanwhile: nothing pops). A deterministic Crowd stub
//     exits it 20 s later through the ordinary Hide(Exit) with the rustle on every machine, and no machine draws the body
//     before the leaf crossing (never an instant un-hide).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Server/Roster/StealthRosterStore.h"
#include "Server/StealthCompositionRoot.h"
#include "Server/Visibility/StealthConcealmentCull.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/PresentationPieSupport.h"
#include "Tests/PIE/Concealment/ConcealmentPieSupport.h"
#include "View/BushRustle.h"

namespace DisconnectReleasePie
{
	constexpr double CrowdStubDwellSec = 20.0;
	constexpr double GraceCeilingSec = 60.0;
}

NETWORK_TEST_CLASS(DisconnectRelease, "StealthGame.Concealment.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousTimeout;
	inline static int32 PreviousNpcCount = -1;

	AStealthPlayerController* Hider = nullptr;
	AStealthPlayerController* Ally = nullptr;
	AStealthPlayerController* Enemy = nullptr;
	TWeakObjectPtr<AStealthPawn> HiderPawn;
	FParticipantId HiderId;
	int32 MachineA = INDEX_NONE;
	int32 MachineE = INDEX_NONE;
	FBushRuntime Bush;
	uint16 PoseSeqAtDrop = 0;
	double DropAt = 0.0;
	double ReleaseAt = -1.0;
	double AllyClosedAt = -1.0;
	double ExitStart = -1.0;
	bool bLeftConcealedDuringGrace = false;
	bool bPoseChangedDuringGrace = false;
	bool bEnemyCopyDuringGrace = false;
	bool bCulledAfterRelease = false;
	bool bAllyDrawnBeforeExit = false;
	bool bDrawnBeforeCrossing = false;
	uint32 Seq = 0x32000000u;
	double Mark = 0.0;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousTimeout = Timeout->GetString();
			Timeout->Set(TEXT("600"), ECVF_SetByCode);
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

	static URosterStore* Roster() { return URosterStore::Get(StealthAssassinationPie::ServerWorld()); }

	TEST_METHOD(ConcealedDisconnecterStaysHiddenThroughGraceThenExitsAsAnNpc)
	{
		using namespace StealthAssassinationPie;
		using namespace StealthConcealmentPie;
		Network
			.Until(TEXT("three players in Active, bushes built, Concealment bound"), []()
			{
				const UConcealmentService* Service = UConcealmentService::Get(ServerWorld());
				const UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(ServerWorld());
				return RoundActive() && ServerPlayers().Num() == 3 && Service != nullptr && Service->IsBound() && Bushes != nullptr && Bushes->IsBuilt() && Roster() != nullptr;
			})
			.Then(TEXT("roles and placement"), [this]()
			{
				AStealthPlayerController* Pair[2] = { nullptr, nullptr };
				ASSERT_THAT(IsTrue(SplitTeams(Enemy, Pair), TEXT("a 2v1 split")));
				Hider = !Pair[0]->IsLocalController() ? Pair[0] : Pair[1];
				Ally = Hider == Pair[0] ? Pair[1] : Pair[0];
				HiderPawn = Cast<AStealthPawn>(Hider->GetPawn());
				MachineA = StealthPresentationPie::MachineOf(Ally);
				MachineE = StealthPresentationPie::MachineOf(Enemy);
				ASSERT_THAT(IsTrue(!Hider->IsLocalController() && HiderPawn.IsValid() && MachineA != INDEX_NONE && MachineE != INDEX_NONE, TEXT("H is a remote client; machines known")));
				HiderId = Roster()->GetParticipant(HiderPawn.Get());
				ASSERT_THAT(IsTrue(HiderId.IsValid(), TEXT("H's pawn is bound")));
				Bush = UBushRuntimeSubsystem::Get(ServerWorld())->GetBushes()[0];
				PlacePlayerAt(Hider, Bush.Anchor + FVector(130.0, 0.0, 0.0), FRotator(0.0, 180.0, 0.0));
				PlacePlayerAt(Ally, Bush.Anchor + FVector(500.0, 250.0, 0.0), FRotator(0.0, -150.0, 0.0));
				PlacePlayerAt(Enemy, Bush.Anchor + FVector(500.0, -250.0, 0.0), FRotator(0.0, 150.0, 0.0));
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("settle"), [this]() { return ServerNow(ServerWorld()) >= Mark + 1.0; })
			.Then(TEXT("H hides"), [this]()
			{
				const uint16 BushId = Bush.Id;
				AsOwner(HiderPawn.Get(), Seq, [BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("H Concealed and culled"), [this]()
			{
				const UStealthConcealmentCull* Cull = UStealthConcealmentCull::Get(ServerWorld());
				return (StateOf(HiderPawn.Get()) == EConcealState::Concealed && Cull != nullptr && Cull->GetState(HiderPawn.Get()) == EConcealCullState::Culled
					&& ServerNow(ServerWorld()) >= Mark + 1.5) || ServerNow(ServerWorld()) >= Mark + 6.0;
			})
			.Then(TEXT("H's participant drops (Roster Disconnected)"), [this]()
			{
				ASSERT_THAT(IsTrue(StateOf(HiderPawn.Get()) == EConcealState::Concealed, TEXT("H is Concealed before the drop")));
				PoseSeqAtDrop = HiderPawn->GetActionState().Seq;
				// The Session-level drop (Round reads Disconnected) without the engine logout: the pawn is kept for the R17 hold.
				UStealthCompositionRoot::Get(ServerWorld())->DropConnectionForTest(Hider);
				DropAt = ServerNow(ServerWorld());
			})
			.Until(TEXT("through the grace until the Round releases the pawn to the crowd"), [this]()
			{
				const AStealthPawn* H = HiderPawn.Get();
				if (H == nullptr)
				{
					return true;
				}
				const bool bReleased = !Roster()->GetParticipant(const_cast<AStealthPawn*>(H)).IsValid();
				if (!bReleased)
				{
					bLeftConcealedDuringGrace |= StateOf(H) != EConcealState::Concealed;
					bPoseChangedDuringGrace |= H->GetActionState().Seq != PoseSeqAtDrop;
					bEnemyCopyDuringGrace |= MachineE != 0 && CopyByGuid(Worlds()[MachineE], H) != nullptr;
				}
				else if (ReleaseAt < 0.0)
				{
					ReleaseAt = ServerNow(ServerWorld());
				}
				return bReleased || ServerNow(ServerWorld()) >= DropAt + DisconnectReleasePie::GraceCeilingSec;
			})
			.Until(TEXT("the concealed NPC: culled for the former ally; then the crowd stub's dwell"), [this]()
			{
				const AStealthPawn* H = HiderPawn.Get();
				if (H == nullptr || ReleaseAt < 0.0)
				{
					return true;
				}
				const UStealthConcealmentCull* Cull = UStealthConcealmentCull::Get(ServerWorld());
				bCulledAfterRelease |= Cull != nullptr && Cull->GetState(H) == EConcealCullState::Culled && StateOf(H) == EConcealState::Concealed;
				const AStealthPawn* AllyCopy = Cast<AStealthPawn>(CopyByGuid(Worlds()[MachineA], H));
				bAllyDrawnBeforeExit |= IsBodyDrawn(AllyCopy);
				if (AllyClosedAt < 0.0 && MachineA != 0 && AllyCopy == nullptr)
				{
					AllyClosedAt = ServerNow(ServerWorld());
				}
				return ServerNow(ServerWorld()) >= ReleaseAt + DisconnectReleasePie::CrowdStubDwellSec;
			})
			.Then(TEXT("the crowd stub exits it through the ordinary Hide(Exit)"), [this]()
			{
				ASSERT_THAT(IsTrue(HiderPawn.IsValid() && StateOf(HiderPawn.Get()) == EConcealState::Concealed, TEXT("20 s after the release: still Concealed")));
				if (UPawnCommandComponent* Command = HiderPawn->GetPawnCommand())
				{
					Command->Hide(FHideCommand::Exit(0)); // the crowd's server-side call (no player origin: an NPC now)
				}
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("through the exit on every machine"), [this]()
			{
				const AStealthPawn* H = HiderPawn.Get();
				if (H == nullptr)
				{
					return true;
				}
				if (ExitStart < 0.0 && H->GetActionState().Pose == EActionPose::BushExit)
				{
					ExitStart = H->GetActionState().StartServerTime;
				}
				if (ExitStart > 0.0)
				{
					const FConcealmentTuning_Public Tuning;
					const TArray<UWorld*> All = Worlds();
					const int32 MachineH = StealthPresentationPie::MachineOf(Hider);
					for (int32 Machine = 0; Machine < All.Num(); ++Machine)
					{
						if (Machine == MachineH)
						{
							continue; // the test seam keeps the dropped player's client up and possessing: not a viewer a real drop leaves
						}
						const AStealthPawn* Copy = Cast<AStealthPawn>(CopyByGuid(All[Machine], H));
						bDrawnBeforeCrossing |= IsBodyDrawn(Copy) && ServerNow(All[Machine]) < ExitStart + Tuning.BushExitCrossSec - 1e-3;
					}
				}
				return (ExitStart > 0.0 && ServerNow(ServerWorld()) >= ExitStart + 1.6) || ServerNow(ServerWorld()) >= Mark + 4.0;
			})
			.Then(TEXT("assert"), [this]()
			{
				TestRunner->AddInfo(FString::Printf(TEXT("CON16 drop %.3f, release %.3f (grace %.1f s), former ally's actor closed %.3f (machine %d), exit start %.3f"),
					DropAt, ReleaseAt, ReleaseAt - DropAt, AllyClosedAt, MachineA, ExitStart));
				ASSERT_THAT(IsTrue(ReleaseAt > 0.0, TEXT("the Round's grace expiry released the pawn to the crowd")));
				ASSERT_THAT(IsFalse(bLeftConcealedDuringGrace, TEXT("through the grace the pawn stayed Concealed")));
				ASSERT_THAT(IsFalse(bPoseChangedDuringGrace, TEXT("the AI hold issued no Hide (the pawn's action state never changed)")));
				ASSERT_THAT(IsFalse(bEnemyCopyDuringGrace, TEXT("the opponent never held an actor for it during the grace")));
				ASSERT_THAT(IsTrue(bCulledAfterRelease, TEXT("after the release: a concealed NPC, culled")));
				if (MachineA != 0)
				{
					ASSERT_THAT(IsTrue(AllyClosedAt > 0.0, TEXT("the former ally's actor closed (culled for former allies)")));
				}
				ASSERT_THAT(IsFalse(bAllyDrawnBeforeExit, TEXT("the former ally never drew the body between the release and the exit (nothing pops)")));
				ASSERT_THAT(IsTrue(ExitStart > 0.0 && StateOf(HiderPawn.Get()) == EConcealState::Outside, TEXT("the crowd's ordinary Hide(Exit) ran: Outside")));
				ASSERT_THAT(IsFalse(bDrawnBeforeCrossing, TEXT("no machine drew the body before the leaf crossing (never an instant un-hide)")));
				for (int32 Machine = 0; Machine < StealthAssassinationPie::Worlds().Num(); ++Machine)
				{
					const UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(StealthAssassinationPie::Worlds()[Machine]);
					const int32 Pulses = Bushes->GetRustleLog().FilterByPredicate([this](const FBushRustlePulse& P) { return P.Kind == EBushRustleKind::Exit && FMath::IsNearlyEqual(P.StartServerTime, ExitStart, 1e-6); }).Num();
					ASSERT_THAT(IsTrue(Pulses == 1, *FString::Printf(TEXT("machine %d: the exit's rustle (one pulse)"), Machine)));
				}
			});
	}
};

#endif
