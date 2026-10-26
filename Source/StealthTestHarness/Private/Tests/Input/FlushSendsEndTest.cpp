// Story iif-013 (production/epics/input-interaction-framework/story-013-flush-sends-end.md), GDD AC 5 (R4, Integration,
// P FlushSendsEnd): GIVEN Prepare held WHEN (a) the window loses focus, (b) a menu opens, (c) the context swaps, or (d)
// the pawn is unpossessed THEN End is sent within one frame; AND the action restarts only on a fresh press; AND in case
// (b) the owner's Move and Look are zero for as long as the menu is open (no displacement after braking to rest).
// Pinned (QL-STORY-READY rewrite): "within one frame" = the End is sent no later than one frame after the frame the
// event is handled (frame-counter delta <= 1). (a) invokes the builder's focus-loss handler directly (the handler the
// Slate application-deactivation event calls); the OS alt-tab check is DEFERRED to a manual run. (b) switches the Common
// UI router to Menu through SetActiveUIInputConfig (spike V5). (c) swaps IMC_Active for IMC_Informant. (d) the server
// unpossesses the pawn (the owner sees it through OnRep_Pawn). "No displacement after braking to rest": with W held for
// the 3 s the menu is open, the owner's Move intent is zero and the server pawn moves <= 1 cm over the last second (the
// crowd may brush it; 0.1 cm is the story's proposal, recorded). Edge: the engine's later Completed for a flushed press
// is swallowed (no second End).
// CQTest: NETWORK_TEST_CLASS(FlushSendsEnd, "StealthGame.Input.PIE"), listen server + 2 clients; client 1 acts.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "CommonUITypes.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Input/CommonUIActionRouterBase.h"
#include "InputKeyEventArgs.h"
#include "Server/Core/StealthPawnCommandComponent.h"
#include "StealthGameMode.h"
#include "StealthPlayerController.h"
#include "View/Core/StealthPawn.h"
#include "View/Input/PawnCommandClient.h"

namespace FlushSendsEndTestData
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	const TCHAR* const NetworkTimeoutCeilingSeconds = TEXT("300");
	constexpr uint64 SettleFrames = 10;
	/** The menu stays open about 3 s (frames, never wall time). */
	constexpr uint64 MenuOpenFrames = 180;
	constexpr uint64 RestWindowFrames = 60;
	constexpr double RestDisplacementCm = 1.0;

	enum class ECase : uint8 { FocusLoss, Menu, ContextSwap, Unpossess };
	const TCHAR* CaseName(ECase Case)
	{
		switch (Case)
		{
		case ECase::FocusLoss: return TEXT("(a) focus loss");
		case ECase::Menu: return TEXT("(b) menu open");
		case ECase::ContextSwap: return TEXT("(c) context swap");
		default: return TEXT("(d) unpossess");
		}
	}

	struct FSent
	{
		ECommandKind Kind;
		uint64 Frame;
	};
}

NETWORK_TEST_CLASS(FlushSendsEnd, "StealthGame.Input.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	/** CQTest keeps the description pointer: the label strings must outlive the command queue. */
	TArray<FString> Labels;
	const TCHAR* L(FString Label) { return *Labels.Add_GetRef(MoveTemp(Label)); }
	inline static FString PreviousNetworkTimeout;

	TArray<FlushSendsEndTestData::FSent> Sent;
	uint64 EventFrame = 0;
	uint64 Mark = 0;
	FVector RestStart = FVector::ZeroVector;
	double MaxRestDisplacement = 0.0;
	bool bMoveZeroDuringMenu = true;
	TWeakObjectPtr<APawn> UnpossessedPawn;
	TArray<FString> Failures;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(FlushSendsEndTestData::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
	}

	AFTER_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			if (!PreviousNetworkTimeout.IsEmpty())
			{
				Timeout->Set(*PreviousNetworkTimeout, ECVF_SetByCode);
			}
		}
	}

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>()
			.AsListenServer()
			.WithClients(2)
			.WithGameMode(AStealthGameMode::StaticClass())
			.Build(Network);
		Network.StartOnFixtureMap(FlushSendsEndTestData::FixtureMap);
	}

	static UWorld* FirstClientWorld()
	{
		const FWorldContext* Best = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() == NM_Client && (!Best || Context.PIEInstance < Best->PIEInstance))
			{
				Best = &Context;
			}
		}
		return Best ? Best->World() : nullptr;
	}

	static UWorld* ServerWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() == NM_ListenServer)
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	static AStealthPlayerController* Owner() { UWorld* W = FirstClientWorld(); return W ? Cast<AStealthPlayerController>(W->GetFirstPlayerController()) : nullptr; }

	/** The owner's controller on the server (matched by PlayerId). */
	static APlayerController* ServerSideOwner()
	{
		const AStealthPlayerController* Client = Owner();
		UWorld* Server = ServerWorld();
		if (!Client || !Client->PlayerState || !Server)
		{
			return nullptr;
		}
		for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && !PC->IsLocalController() && PC->PlayerState && PC->PlayerState->GetPlayerId() == Client->PlayerState->GetPlayerId())
			{
				return PC;
			}
		}
		return nullptr;
	}

	static void Key(const FKey& InKey, bool bDown)
	{
		if (AStealthPlayerController* PC = Owner())
		{
			PC->InputKey(FInputKeyEventArgs::CreateSimulated(InKey, bDown ? IE_Pressed : IE_Released, bDown ? 1.f : 0.f));
		}
	}

	static UCommonUIActionRouterBase* Router()
	{
		AStealthPlayerController* PC = Owner();
		return PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UCommonUIActionRouterBase>() : nullptr;
	}

	int32 Count(ECommandKind Kind, uint64 SinceFrame) const
	{
		return Sent.FilterByPredicate([Kind, SinceFrame](const FlushSendsEndTestData::FSent& S) { return S.Kind == Kind && S.Frame >= SinceFrame; }).Num();
	}

	void Fail(const FString& Message) { Failures.Add(Message); }

	void QueueCase(FlushSendsEndTestData::ECase Case)
	{
		using namespace FlushSendsEndTestData;
		const FString Name = CaseName(Case);
		Network
			.Then(L(Name + TEXT(": hold Prepare")), [this]() { Key(EKeys::Q, true); Mark = GFrameCounter; })
			.Until(L(Name + TEXT(": Begin sent")), [this]() { return GFrameCounter >= Mark + SettleFrames; })
			.Then(L(Name + TEXT(": the event")), [this, Case, Name]()
			{
				if (Count(ECommandKind::PrepareBegin, Mark) != 1)
				{
					Fail(FString::Printf(TEXT("%s: expected one PrepareBegin before the event, got %d"), *Name, Count(ECommandKind::PrepareBegin, Mark)));
				}
				EventFrame = GFrameCounter;
				switch (Case)
				{
				case ECase::FocusLoss:
					Owner()->GetCommandClient()->NotifyFocusLost();
					break;
				case ECase::Menu:
					Key(EKeys::W, true);
					if (UCommonUIActionRouterBase* R = Router())
					{
						R->SetActiveUIInputConfig(FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture));
					}
					break;
				case ECase::ContextSwap:
					Owner()->GetCommandClient()->SetInformantContext(true);
					break;
				case ECase::Unpossess:
					if (APlayerController* Server = ServerSideOwner())
					{
						UnpossessedPawn = Server->GetPawn();
						Server->UnPossess();
					}
					break;
				}
			});
		if (Case == ECase::Unpossess)
		{
			Network.Until(L(Name + TEXT(": the owner sees the unpossession")), [this]() { return Owner() && Owner()->GetPawn() == nullptr; })
				.Then(L(Name + TEXT(": handled frame")), [this]() { EventFrame = GFrameCounter; });
		}
		if (Case == ECase::Menu)
		{
			Network.Until(L(Name + TEXT(": the menu stays open (W held)")), [this]()
			{
				const AStealthPawn* Pawn = Owner() ? Cast<AStealthPawn>(Owner()->GetPawn()) : nullptr;
				const UPawnCommandComponent* Command = Pawn ? Pawn->GetPawnCommand() : nullptr;
				bMoveZeroDuringMenu &= Command && Command->GetMoveIntent().IsZero();
				APlayerController* Server = ServerSideOwner();
				const APawn* ServerPawn = Server ? Server->GetPawn() : nullptr;
				if (ServerPawn && GFrameCounter == EventFrame + FlushSendsEndTestData::MenuOpenFrames - FlushSendsEndTestData::RestWindowFrames)
				{
					RestStart = ServerPawn->GetActorLocation();
				}
				if (ServerPawn && GFrameCounter > EventFrame + FlushSendsEndTestData::MenuOpenFrames - FlushSendsEndTestData::RestWindowFrames)
				{
					MaxRestDisplacement = FMath::Max(MaxRestDisplacement, FVector::Dist2D(ServerPawn->GetActorLocation(), RestStart));
				}
				return GFrameCounter >= EventFrame + FlushSendsEndTestData::MenuOpenFrames;
			});
		}
		else
		{
			Network.Until(L(Name + TEXT(": settle")), [this]() { return GFrameCounter >= EventFrame + SettleFrames; });
		}
		Network
			.Then(L(Name + TEXT(": one End within a frame; held key does not restart")), [this, Case, Name]()
			{
				const TArray<FSent> Ends = Sent.FilterByPredicate([this](const FSent& S) { return S.Kind == ECommandKind::PrepareEnd && S.Frame >= EventFrame - 1; });
				if (Ends.Num() != 1 || Ends[0].Frame > EventFrame + 1)
				{
					Fail(FString::Printf(TEXT("%s: %d End(s), first at event+%lld"), *Name, Ends.Num(), Ends.Num() ? static_cast<int64>(Ends[0].Frame) - static_cast<int64>(EventFrame) : -1));
				}
				if (Count(ECommandKind::PrepareBegin, EventFrame) != 0)
				{
					Fail(FString::Printf(TEXT("%s: the held key restarted the action"), *Name));
				}
				Key(EKeys::Q, false);
				Key(EKeys::W, false);
				Mark = GFrameCounter;
			})
			.Until(L(Name + TEXT(": release settles")), [this]() { return GFrameCounter >= Mark + SettleFrames; })
			.Then(L(Name + TEXT(": no second End on release; restore")), [this, Case, Name]()
			{
				if (Count(ECommandKind::PrepareEnd, EventFrame - 1) != 1)
				{
					Fail(FString::Printf(TEXT("%s: the release produced a second End"), *Name));
				}
				if (Case == ECase::Menu)
				{
					if (UCommonUIActionRouterBase* R = Router())
					{
						R->SetActiveUIInputConfig(FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently));
					}
					if (!bMoveZeroDuringMenu)
					{
						Fail(TEXT("(b): the owner's Move was not zero while the menu was open"));
					}
					if (MaxRestDisplacement > FlushSendsEndTestData::RestDisplacementCm)
					{
						Fail(FString::Printf(TEXT("(b): the pawn moved %.2f cm after braking to rest"), MaxRestDisplacement));
					}
					TestRunner->AddInfo(FString::Printf(TEXT("FLUSH (b) rest displacement over the last second: %.3f cm"), MaxRestDisplacement));
				}
				if (Case == ECase::ContextSwap)
				{
					Owner()->GetCommandClient()->SetInformantContext(false);
				}
				if (Case == ECase::Unpossess)
				{
					if (APlayerController* Server = ServerSideOwner())
					{
						Server->Possess(UnpossessedPawn.Get());
					}
				}
				Mark = GFrameCounter;
			});
		if (Case == ECase::Unpossess)
		{
			Network.Until(L(Name + TEXT(": re-possessed")), [this]() { return Owner() && Owner()->GetPawn() != nullptr && GFrameCounter > Mark + SettleFrames; });
		}
		else
		{
			Network.Until(L(Name + TEXT(": restored")), [this]() { return GFrameCounter >= Mark + SettleFrames; });
		}
		Network
			.Then(L(Name + TEXT(": a fresh press")), [this]() { Key(EKeys::Q, true); Mark = GFrameCounter; })
			.Until(L(Name + TEXT(": fresh Begin")), [this]() { return GFrameCounter >= Mark + SettleFrames; })
			.Then(L(Name + TEXT(": the fresh press began the action")), [this, Name]()
			{
				if (Count(ECommandKind::PrepareBegin, Mark) != 1)
				{
					Fail(FString::Printf(TEXT("%s: a fresh press did not begin the action"), *Name));
				}
				Key(EKeys::Q, false);
				Mark = GFrameCounter;
			})
			.Until(L(Name + TEXT(": done")), [this]() { return GFrameCounter >= Mark + SettleFrames; });
	}

	TEST_METHOD(EveryFlushEventSendsOneEndWithinAFrame)
	{
		using namespace FlushSendsEndTestData;
		Network
			.Until(TEXT("client 1 possesses its pawn"), []() { return Owner() && Cast<AStealthPawn>(Owner()->GetPawn()) != nullptr && Router() != nullptr; })
			.Then(TEXT("record every command the owner sends"), [this]()
			{
				Owner()->GetCommandClient()->OnOwnerCommandPredicted.AddLambda([this](ECommandKind Kind, FClientCommandSeq) { Sent.Add({ Kind, GFrameCounter }); });
			});
		QueueCase(ECase::FocusLoss);
		QueueCase(ECase::Menu);
		QueueCase(ECase::ContextSwap);
		QueueCase(ECase::Unpossess);
		Network.Then(TEXT("verdict"), [this]()
		{
			ASSERT_THAT(IsTrue(Failures.IsEmpty(), *FString::Join(Failures, TEXT("; "))));
		});
	}
};

#endif
