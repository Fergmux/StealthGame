// Story iif-092 (production/epics/input-interaction-framework/story-092-common-ui-mid-brace-single-end.md), ADR-0011
// Validation Criteria "Common UI test": GIVEN an owner holding Prepare (a brace active) with UPawnCommandClient
// subscribed to the router's input-mode change WHEN a menu opens (input mode changes to Menu) THEN exactly one End is
// sent, in the same call stack as the mode change and before the next tick; AND no second End is sent when the engine
// later reports a release for the held key.
// "Same call stack": the End is sent during the router's SetActiveUIInputConfig call itself (counted before and
// after the call returns; the builder's End runs inside the router's OnActiveInputModeChanged broadcast). "Before the next
// tick": the End's frame equals the mode-change frame and precedes the router's FlushPressedKeys (next tick). The
// server sees exactly one PrepareEnd at ingress. The mode change uses SetActiveUIInputConfig (spike V5: the router's
// own path; ADR-0021's menu stack ends holds itself before a push). 5 repetitions.
// CQTest: NETWORK_TEST_CLASS(CommonUiMidBraceSingleEnd, "StealthGame.Input.PIE"), listen server + 1 client.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "CommonUITypes.h"
#include "Engine/LocalPlayer.h"
#include "HAL/IConsoleManager.h"
#include "Input/CommandLog.h"
#include "Input/CommonUIActionRouterBase.h"
#include "InputKeyEventArgs.h"
#include "StealthGameMode.h"
#include "StealthPlayerController.h"
#include "StealthGameState.h"
#include "View/Core/StealthPawn.h"
#include "View/Input/PawnCommandClient.h"


namespace MidBraceRoundGate
{
	/** The client's replicated Round phase is Active (R6: pawns exist in Preparing, but commands are NotNow until Active). */
	inline bool IsClientRoundActive(const APlayerController* PC)
	{
		const AStealthGameState* GS = PC && PC->GetWorld() ? PC->GetWorld()->GetGameState<AStealthGameState>() : nullptr;
		return GS != nullptr && GS->GetRoundPublicState().Phase == ERoundPhase::Active;
	}
}

namespace CommonUiMidBraceSingleEndTestData
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	const TCHAR* const NetworkTimeoutCeilingSeconds = TEXT("240");
	constexpr int32 Repetitions = 5;
	constexpr uint64 SettleFrames = 12;
}

NETWORK_TEST_CLASS(CommonUiMidBraceSingleEnd, "StealthGame.Input.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	/** CQTest keeps the description pointer: the label strings must outlive the command queue. */
	TArray<FString> Labels;
	const TCHAR* L(FString Label) { return *Labels.Add_GetRef(MoveTemp(Label)); }
	inline static FString PreviousNetworkTimeout;

	TUniquePtr<FStealthCommandLog> Log;
	int32 Ends = 0;
	int32 EndsInsideCallback = 0;
	uint64 EndFrame = 0;
	uint64 CallbackFrame = 0;
	uint64 FlushMark = 0;
	uint64 Mark = 0;
	bool bInsideCallback = false;
	TArray<FString> Failures;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(CommonUiMidBraceSingleEndTestData::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
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
			.WithClients(1)
			.WithGameMode(AStealthGameMode::StaticClass())
			.Build(Network);
		Network.StartOnFixtureMap(CommonUiMidBraceSingleEndTestData::FixtureMap);
	}

	AFTER_EACH()
	{
		Log.Reset();
	}

	static AStealthPlayerController* Owner()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() == NM_Client)
			{
				return Cast<AStealthPlayerController>(Context.World()->GetFirstPlayerController());
			}
		}
		return nullptr;
	}

	static UCommonUIActionRouterBase* Router()
	{
		AStealthPlayerController* PC = Owner();
		return PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UCommonUIActionRouterBase>() : nullptr;
	}

	static void Key(bool bDown)
	{
		if (AStealthPlayerController* PC = Owner())
		{
			PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Q, bDown ? IE_Pressed : IE_Released, bDown ? 1.f : 0.f));
		}
	}

	TEST_METHOD(MenuOpenSendsExactlyOneSynchronousEnd)
	{
		using namespace CommonUiMidBraceSingleEndTestData;
		Network
			.Until(TEXT("the client possesses its pawn in an Active round"), []() { return Owner() && Cast<AStealthPawn>(Owner()->GetPawn()) != nullptr && Router() != nullptr && MidBraceRoundGate::IsClientRoundActive(Owner()); })
			.Then(TEXT("observe the Ends and the callback"), [this]()
			{
				Log = MakeUnique<FStealthCommandLog>();
				Owner()->GetCommandClient()->OnOwnerCommandPredicted.AddLambda([this](ECommandKind Kind, FClientCommandSeq)
				{
					if (Kind == ECommandKind::PrepareEnd)
					{
						++Ends;
						EndFrame = GFrameCounter;
					}
				});
				Router()->OnActiveInputModeChanged().AddLambda([this](ECommonInputMode Mode)
				{
					if (Mode == ECommonInputMode::Menu)
					{
						CallbackFrame = GFrameCounter;
					}
				});
			});

		for (int32 Run = 0; Run < Repetitions; ++Run)
		{
			Network
				.Then(L(FString::Printf(TEXT("run %d: brace (hold Prepare)"), Run)), [this]() { Ends = 0; EndFrame = 0; CallbackFrame = 0; EndsInsideCallback = 0; Log->Reset(); Key(true); Mark = GFrameCounter; })
				.Until(L(FString::Printf(TEXT("run %d: the brace reached the server"), Run)), [this]()
				{
					return Log->KindsAt(EStealthCommandLogPoint::QueueSubmit).Contains(ECommandKind::PrepareBegin) || GFrameCounter > Mark + 300;
				})
				.Then(L(FString::Printf(TEXT("run %d: the menu opens"), Run)), [this]()
				{
					FlushMark = AStealthPlayerController::GetFlushPressedKeysTrace().Num();
					const int32 EndsBefore = Ends;
					Router()->SetActiveUIInputConfig(FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture));
					EndsInsideCallback = Ends - EndsBefore;
					Mark = GFrameCounter;
				})
				.Until(L(FString::Printf(TEXT("run %d: settle"), Run)), [this]() { return GFrameCounter >= Mark + SettleFrames; })
				.Then(L(FString::Printf(TEXT("run %d: release the key"), Run)), [this]() { Key(false); Mark = GFrameCounter; })
				.Until(L(FString::Printf(TEXT("run %d: release settles"), Run)), [this]() { return GFrameCounter >= Mark + SettleFrames; })
				.Then(L(FString::Printf(TEXT("run %d: check and close the menu"), Run)), [this, Run]()
				{
					const TArray<uint64>& Flushes = AStealthPlayerController::GetFlushPressedKeysTrace();
					const uint64 FlushFrame = Flushes.Num() > static_cast<int32>(FlushMark) ? Flushes[FlushMark] : 0;
					const int32 ServerEnds = Log->KindsAt(EStealthCommandLogPoint::IngressOutput).FilterByPredicate([](ECommandKind K) { return K == ECommandKind::PrepareEnd; }).Num();
					TestRunner->AddInfo(FString::Printf(TEXT("MIDBRACE run %d: ends %d (inside the mode-change call %d), callback %llu end %llu flush %llu, server PrepareEnd %d"),
						Run, Ends, EndsInsideCallback, CallbackFrame, EndFrame, FlushFrame, ServerEnds));
					if (Ends != 1 || EndsInsideCallback != 1 || EndFrame != CallbackFrame || (FlushFrame != 0 && FlushFrame <= EndFrame) || ServerEnds != 1)
					{
						Failures.Add(FString::Printf(TEXT("run %d: ends %d inside %d, end %llu callback %llu flush %llu, server %d"), Run, Ends, EndsInsideCallback, EndFrame, CallbackFrame, FlushFrame, ServerEnds));
					}
					Router()->SetActiveUIInputConfig(FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently));
					Mark = GFrameCounter;
				})
				.Until(L(FString::Printf(TEXT("run %d: menu closed"), Run)), [this]() { return GFrameCounter >= Mark + SettleFrames; });
		}
		Network.Then(TEXT("verdict"), [this]() { ASSERT_THAT(IsTrue(Failures.IsEmpty(), *FString::Join(Failures, TEXT("; ")))); });
	}
};

#endif
