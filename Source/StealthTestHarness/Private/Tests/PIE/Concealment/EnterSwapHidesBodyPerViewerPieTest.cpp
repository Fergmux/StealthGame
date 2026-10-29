// Concealment story con-028 AC1 (production/epics/concealment-bushes/story-028-enter-swap-hides-body-per-viewer.md; GDD
// AC-CON-01a; ADR-0017 sections 5 and 6): StealthGame.Concealment.PIE.EnterSwapHidesBodyPerViewer.<method>. Listen host +
// 2 clients on L_Prototype, NpcCount 0, no net emulation (AC-CON-01a carries no [Net] tag). The hider H walks into a bush
// through the one Hide(Enter); its ally A and the opponent E stand with line of sight 5 m away. Every frame, on every
// machine, the harness logs that machine's EstimatedServerNow() beside whether H's body (the mesh) and every attached
// primitive (attachments: their shadows and reflections follow the primitive's visibility, ADR-0017 section 6, the R5
// flag set) are drawn. The swap key is FActionState.StartServerTime + BushEnterSwapSec.
//   BodyAndAttachmentsVanishOnTheSwapFrameForOpponentAndAlly: on A's and E's machines the body and every attachment are
//     drawn on every frame before the key and hidden from the first frame at or after it (that frame +- one 60 Hz frame);
//     afterwards A's copy reports IsAllyBodyHidden (the hatched marker's input) and draws no body; H's own machine keeps
//     its body throughout. NEGATIVE CONTROL: the same assertion against the key shifted by 5 frames fails on both machines.
// 1P / 3P: the hide is a primitive visibility write, independent of the viewer's camera mode (the eye-origin mask that
// applies in third person is Camera story 015); the run-and-observe captures both views (con-enter-*.png).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/OwnPresentationPieSupport.h"
#include "Tests/PIE/Assassination/PresentationPieSupport.h"
#include "Tests/PIE/Concealment/ConcealmentPieSupport.h"
#include "View/AllyBodyHidden.h"

namespace EnterSwapPie
{
	/** One sampled frame of one machine. */
	struct FSample
	{
		double Now = 0.0;
		bool bExists = false;
		bool bBodyDrawn = false;
		bool bAnyPrimitiveDrawn = false;
	};

	/** True when any visible primitive of Copy (mesh and attachments, not the collision capsule) is drawn. */
	inline bool AnyPrimitiveDrawn(const AStealthPawn* Copy)
	{
		TArray<UPrimitiveComponent*> Primitives;
		Copy->GetComponents<UPrimitiveComponent>(Primitives);
		for (const UPrimitiveComponent* Primitive : Primitives)
		{
			if (Primitive != nullptr && Primitive != Copy->GetCapsuleComponent() && Primitive->IsVisible() && !Primitive->bHiddenInGame)
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * The swap check on one machine's samples against Key: every sample before Key draws the body (and some primitive), and
	 * the first sample with no primitive drawn lies on the frame of Key (never before it, at most one frame after it).
	 */
	inline bool SwapOnFrame(const TArray<FSample>& Samples, double Key, double Frame, FString& OutWhy)
	{
		double FirstHidden = -1.0;
		for (const FSample& S : Samples)
		{
			if (!S.bExists)
			{
				continue;
			}
			if (S.Now < Key - 1e-3 && !(S.bBodyDrawn && S.bAnyPrimitiveDrawn))
			{
				OutWhy = FString::Printf(TEXT("hidden at %.4f before the key %.4f"), S.Now, Key);
				return false;
			}
			if (FirstHidden < 0.0 && !S.bBodyDrawn && !S.bAnyPrimitiveDrawn)
			{
				FirstHidden = S.Now;
			}
			if (FirstHidden >= 0.0 && (S.bBodyDrawn || S.bAnyPrimitiveDrawn))
			{
				OutWhy = FString::Printf(TEXT("drawn again at %.4f after the hide at %.4f"), S.Now, FirstHidden);
				return false;
			}
		}
		OutWhy = FString::Printf(TEXT("first hidden frame %.4f, key %.4f, frame %.4f"), FirstHidden, Key, Frame);
		return FirstHidden >= Key - 1e-3 && FirstHidden <= Key + Frame;
	}
}

NETWORK_TEST_CLASS(EnterSwapHidesBodyPerViewer, "StealthGame.Concealment.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousTimeout;
	inline static int32 PreviousNpcCount = -1;

	AStealthPlayerController* Hider = nullptr;
	AStealthPlayerController* Ally = nullptr;
	AStealthPlayerController* Enemy = nullptr;
	int32 MachineH = INDEX_NONE;
	int32 MachineA = INDEX_NONE;
	int32 MachineE = INDEX_NONE;
	FBushRuntime Bush;
	TArray<EnterSwapPie::FSample> Samples[3];
	StealthOwnPresentationPie::FFrameTracker Frames;
	double Start = -1.0;
	uint32 Seq = 0x28000000u;
	double Mark = 0.0;
	bool bAllyMarkerAfter = false;
	bool bAllyBodyAfter = true;
	bool bOwnBodyAlways = true;

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

	void Sample()
	{
		using namespace StealthConcealmentPie;
		Frames.Sample();
		const TArray<UWorld*> All = StealthAssassinationPie::Worlds();
		const APawn* H = Hider->GetPawn();
		for (int32 Machine = 0; Machine < All.Num() && Machine < 3; ++Machine)
		{
			const AStealthPawn* Copy = Cast<AStealthPawn>(CopyByGuid(All[Machine], H));
			EnterSwapPie::FSample& S = Samples[Machine].AddDefaulted_GetRef();
			S.Now = StealthAssassinationPie::ServerNow(All[Machine]);
			S.bExists = Copy != nullptr;
			S.bBodyDrawn = IsBodyDrawn(Copy);
			S.bAnyPrimitiveDrawn = Copy != nullptr && EnterSwapPie::AnyPrimitiveDrawn(Copy);
			if (Machine == MachineH)
			{
				bOwnBodyAlways &= S.bBodyDrawn;
			}
		}
	}

	TEST_METHOD(BodyAndAttachmentsVanishOnTheSwapFrameForOpponentAndAlly)
	{
		using namespace StealthAssassinationPie;
		using namespace StealthConcealmentPie;
		Network
			.Until(TEXT("three players in Active, bushes built everywhere, Concealment bound"), []()
			{
				const UConcealmentService* Service = UConcealmentService::Get(ServerWorld());
				if (!RoundActive() || ServerPlayers().Num() != 3 || Service == nullptr || !Service->IsBound())
				{
					return false;
				}
				for (UWorld* World : Worlds())
				{
					const UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(World);
					if (Bushes == nullptr || !Bushes->IsBuilt())
					{
						return false;
					}
				}
				return true;
			})
			.Then(TEXT("roles (H a remote client) and line-of-sight placement"), [this]()
			{
				AStealthPlayerController* Pair[2] = { nullptr, nullptr };
				ASSERT_THAT(IsTrue(SplitTeams(Enemy, Pair), TEXT("a 2v1 split")));
				Hider = !Pair[0]->IsLocalController() ? Pair[0] : Pair[1];
				Ally = Hider == Pair[0] ? Pair[1] : Pair[0];
				MachineH = StealthPresentationPie::MachineOf(Hider);
				MachineA = StealthPresentationPie::MachineOf(Ally);
				MachineE = StealthPresentationPie::MachineOf(Enemy);
				ASSERT_THAT(IsTrue(!Hider->IsLocalController() && MachineH != INDEX_NONE && MachineA != INDEX_NONE && MachineE != INDEX_NONE, TEXT("H is a remote client; every machine is known")));
				Bush = UBushRuntimeSubsystem::Get(ServerWorld())->GetBushes()[0];
				PlacePlayerAt(Hider, Bush.Anchor + FVector(130.0, 0.0, 0.0), FRotator(0.0, 180.0, 0.0));
				PlacePlayerAt(Ally, Bush.Anchor + FVector(500.0, 250.0, 0.0), FRotator(0.0, -150.0, 0.0));
				PlacePlayerAt(Enemy, Bush.Anchor + FVector(500.0, -250.0, 0.0), FRotator(0.0, 150.0, 0.0));
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("settle 1 s"), [this]() { Frames.Sample(); return ServerNow(ServerWorld()) >= Mark + 1.0; })
			.Then(TEXT("H hides"), [this]()
			{
				const uint16 BushId = Bush.Id;
				AsOwner(Cast<AStealthPawn>(Hider->GetPawn()), Seq, [BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("frame-stepped capture through the swap and the cull"), [this]()
			{
				Sample();
				const AStealthPawn* H = Cast<AStealthPawn>(Hider->GetPawn());
				if (Start < 0.0 && H != nullptr && H->GetActionState().Pose == EActionPose::BushEnter)
				{
					Start = H->GetActionState().StartServerTime;
				}
				return (Start > 0.0 && ServerNow(ServerWorld()) >= Start + 1.4) || ServerNow(ServerWorld()) >= Mark + 4.0;
			})
			.Then(TEXT("the ally's marker input after the swap"), [this]()
			{
				const AStealthPawn* Copy = Cast<AStealthPawn>(CopyByGuid(Worlds()[MachineA], Hider->GetPawn()));
				bAllyMarkerAfter = Copy != nullptr && IsAllyBodyHidden(Copy);
				bAllyBodyAfter = IsBodyDrawn(Copy);
			})
			.Then(TEXT("assert the swap frame, the ally, the owner and the negative control"), [this]()
			{
				const FConcealmentTuning_Public Tuning;
				ASSERT_THAT(IsTrue(Start > 0.0, TEXT("H's BushEnter pose was written")));
				const double Key = Start + Tuning.BushEnterSwapSec;
				const double Shifted = Key + 5.0 / 60.0;
				for (const int32 Machine : { MachineE, MachineA })
				{
					const TCHAR* Who = Machine == MachineE ? TEXT("opponent") : TEXT("ally");
					const double Frame = FMath::Max(1.0 / 60.0, Frames.FrameOf(Machine));
					FString Why;
					const bool bOnKey = EnterSwapPie::SwapOnFrame(Samples[Machine], Key, Frame, Why);
					TestRunner->AddInfo(FString::Printf(TEXT("CON01a %s (machine %d): %s; %d samples"), Who, Machine, *Why, Samples[Machine].Num()));
					ASSERT_THAT(IsTrue(bOnKey, *FString::Printf(TEXT("%s: body and attachments vanish on the swap frame (+- 1 frame)"), Who)));
					FString ShiftedWhy;
					const bool bOnShifted = EnterSwapPie::SwapOnFrame(Samples[Machine], Shifted, Frame, ShiftedWhy);
					TestRunner->AddInfo(FString::Printf(TEXT("CON01a %s negative control (key + 5 frames): %s -> %s"), Who, *ShiftedWhy, bOnShifted ? TEXT("passes (the capture is blind)") : TEXT("FAILS (as it must)")));
					ASSERT_THAT(IsFalse(bOnShifted, *FString::Printf(TEXT("%s: the same check against the key shifted by 5 frames fails"), Who)));
				}
				ASSERT_THAT(IsTrue(bAllyMarkerAfter && !bAllyBodyAfter, TEXT("the ally then shows the hatched marker input and no body")));
				ASSERT_THAT(IsTrue(bOwnBodyAlways, TEXT("the owner keeps its own body on every frame")));
			});
	}
};

#endif
