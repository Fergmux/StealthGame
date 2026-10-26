// Story iif-015 (production/epics/input-interaction-framework/story-015-default-bindings.md), GDD AC 30 (R15,
// Integration, P DefaultBindings): GIVEN fresh settings WHEN each binding in the R15 table is pressed on keyboard/mouse
// and on gamepad THEN the mapped command reaches the server command log; AND RMB is not bound to Prepare.
// Per-row rewrite (QL-STORY-READY, pinned here): Traverse, Interact (Begin/End), Prepare (Begin/End), Attack and Hide
// produce their command in the server command log (ingress output); Move produces a Move intent on the owner's command
// component and a ServerMove velocity on the server; Look changes the control rotation; Run sets the Run gait; Camera
// toggle, Shoulder swap and Walk slower produce 0 server commands and their local effect; Ping produces the controller's
// ServerTeamPing RPC. "Fresh settings": no UEnhancedInputUserSettings profile is registered by the project (rebinding is
// not in the MVP), so the IMC data is what is mapped. Keys are simulated key events on the client controller.
// CQTest: NETWORK_TEST_CLASS(DefaultBindings, "StealthGame.Input.PIE"), listen server + 2 clients (client 1 presses).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Input/CommandLog.h"
#include "Input/RpcSpy.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "Server/Core/StealthPawnCommandComponent.h"
#include "StealthGameMode.h"
#include "StealthPlayerController.h"
#include "View/Core/StealthPawn.h"
#include "View/Input/PawnCommandClient.h"

namespace DefaultBindingsTestData
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	const TCHAR* const NetworkTimeoutCeilingSeconds = TEXT("400");
	constexpr uint64 HoldFrames = 20;
	constexpr uint64 SettleFrames = 25;

	enum class EExpect : uint8 { Commands, Move, Look, Run, CameraToggle, ShoulderSwap, WalkSlower, Ping };

	struct FRow
	{
		const TCHAR* Name;
		FKey Key;
		bool bAxis;
		float AxisValue;
		EExpect Expect;
		TArray<ECommandKind> Kinds;
	};

	TArray<FRow> Rows()
	{
		return {
			{ TEXT("Traverse / Space"), EKeys::SpaceBar, false, 0.f, EExpect::Commands, { ECommandKind::Traverse } },
			{ TEXT("Traverse / A"), EKeys::Gamepad_FaceButton_Bottom, false, 0.f, EExpect::Commands, { ECommandKind::Traverse } },
			{ TEXT("Interact / E"), EKeys::E, false, 0.f, EExpect::Commands, { ECommandKind::InteractBegin, ECommandKind::InteractEnd } },
			{ TEXT("Interact / X"), EKeys::Gamepad_FaceButton_Left, false, 0.f, EExpect::Commands, { ECommandKind::InteractBegin, ECommandKind::InteractEnd } },
			{ TEXT("Prepare / Q"), EKeys::Q, false, 0.f, EExpect::Commands, { ECommandKind::PrepareBegin, ECommandKind::PrepareEnd } },
			{ TEXT("Prepare / LT"), EKeys::Gamepad_LeftTrigger, false, 0.f, EExpect::Commands, { ECommandKind::PrepareBegin, ECommandKind::PrepareEnd } },
			{ TEXT("Attack / LMB"), EKeys::LeftMouseButton, false, 0.f, EExpect::Commands, { ECommandKind::Attack } },
			{ TEXT("Attack / RT"), EKeys::Gamepad_RightTrigger, false, 0.f, EExpect::Commands, { ECommandKind::Attack } },
			{ TEXT("Hide / F"), EKeys::F, false, 0.f, EExpect::Commands, { ECommandKind::HideEnter } },
			{ TEXT("Hide / B"), EKeys::Gamepad_FaceButton_Right, false, 0.f, EExpect::Commands, { ECommandKind::HideEnter } },
			{ TEXT("Move / W"), EKeys::W, false, 0.f, EExpect::Move, {} },
			{ TEXT("Move / left stick"), EKeys::Gamepad_LeftY, true, 1.f, EExpect::Move, {} },
			{ TEXT("Look / mouse"), EKeys::MouseX, true, 8.f, EExpect::Look, {} },
			{ TEXT("Look / right stick"), EKeys::Gamepad_RightX, true, 1.f, EExpect::Look, {} },
			{ TEXT("Run / Left Shift"), EKeys::LeftShift, false, 0.f, EExpect::Run, {} },
			{ TEXT("Run / L3"), EKeys::Gamepad_LeftThumbstick, false, 0.f, EExpect::Run, {} },
			{ TEXT("Camera toggle / V"), EKeys::V, false, 0.f, EExpect::CameraToggle, {} },
			{ TEXT("Camera toggle / Y"), EKeys::Gamepad_FaceButton_Top, false, 0.f, EExpect::CameraToggle, {} },
			{ TEXT("Shoulder swap / X"), EKeys::X, false, 0.f, EExpect::ShoulderSwap, {} },
			{ TEXT("Shoulder swap / middle mouse"), EKeys::MiddleMouseButton, false, 0.f, EExpect::ShoulderSwap, {} },
			{ TEXT("Shoulder swap / D-pad right"), EKeys::Gamepad_DPad_Right, false, 0.f, EExpect::ShoulderSwap, {} },
			{ TEXT("Walk slower / Left Ctrl"), EKeys::LeftControl, false, 0.f, EExpect::WalkSlower, {} },
			{ TEXT("Walk slower / Left Alt"), EKeys::LeftAlt, false, 0.f, EExpect::WalkSlower, {} },
			{ TEXT("Ping / G"), EKeys::G, false, 0.f, EExpect::Ping, {} },
			{ TEXT("Ping / D-pad up"), EKeys::Gamepad_DPad_Up, false, 0.f, EExpect::Ping, {} },
		};
	}
}

NETWORK_TEST_CLASS(DefaultBindings, "StealthGame.Input.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	/** CQTest keeps the description pointer: the label strings must outlive the command queue. */
	TArray<FString> Labels;
	const TCHAR* L(FString Label) { return *Labels.Add_GetRef(MoveTemp(Label)); }
	inline static FString PreviousNetworkTimeout;

	TUniquePtr<FStealthCommandLog> Log;
	TUniquePtr<FOutgoingRpcSpy> Spy;
	uint64 Mark = 0;
	int32 Toggles = 0;
	int32 Swaps = 0;
	bool bObserved = false;
	FRotator RotationBefore;
	TArray<FString> Failures;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(DefaultBindingsTestData::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
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
		Network.StartOnFixtureMap(DefaultBindingsTestData::FixtureMap);
	}

	AFTER_EACH()
	{
		Spy.Reset();
		Log.Reset();
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

	static AStealthPlayerController* Presser() { UWorld* W = FirstClientWorld(); return W ? Cast<AStealthPlayerController>(W->GetFirstPlayerController()) : nullptr; }

	static void Send(const DefaultBindingsTestData::FRow& Row, bool bDown)
	{
		AStealthPlayerController* PC = Presser();
		if (!PC)
		{
			return;
		}
		if (Row.bAxis)
		{
			FInputKeyEventArgs Args = FInputKeyEventArgs::CreateSimulated(Row.Key, IE_Axis, bDown ? Row.AxisValue : 0.f);
			Args.NumSamples = 1;
			PC->InputKey(Args);
		}
		else
		{
			PC->InputKey(FInputKeyEventArgs::CreateSimulated(Row.Key, bDown ? IE_Pressed : IE_Released, bDown ? 1.f : 0.f));
		}
	}

	void Fail(const FString& Message) { Failures.Add(Message); }

	TEST_METHOD(EveryDefaultBindingReachesItsTarget)
	{
		using namespace DefaultBindingsTestData;
		Network
			.Until(TEXT("client 1 possesses its pawn"), []() { return Presser() && Cast<AStealthPawn>(Presser()->GetPawn()) != nullptr; })
			.Then(TEXT("RMB is not bound to Prepare; install the log and the spy"), [this]()
			{
				const UInputMappingContext* Active = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/StealthGame/Input/IMC_Active.IMC_Active"));
				const UInputMappingContext* Common = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/StealthGame/Input/IMC_Common.IMC_Common"));
				ASSERT_THAT(IsTrue(Active && Common, TEXT("the contexts load")));
				for (const UInputMappingContext* Context : { Active, Common })
				{
					for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
					{
						ASSERT_THAT(IsFalse(Mapping.Key == EKeys::RightMouseButton && Mapping.Action && Mapping.Action->GetName() == TEXT("IA_Prepare"), TEXT("RMB is not bound to Prepare")));
					}
				}
				Log = MakeUnique<FStealthCommandLog>();
				Spy = MakeUnique<FOutgoingRpcSpy>(FirstClientWorld());
				Presser()->GetCommandClient()->OnCameraToggleAccepted.AddLambda([this]() { ++Toggles; });
				Presser()->GetCommandClient()->OnShoulderSwap.AddLambda([this]() { ++Swaps; });
			});

		for (const FRow& Row : Rows())
		{
			Network
				.Then(L(FString::Printf(TEXT("%s: press"), Row.Name)), [this, Row]()
				{
					Log->Reset();
					Spy->Records.Reset();
					Toggles = 0;
					Swaps = 0;
					bObserved = false;
					RotationBefore = Presser()->GetControlRotation();
					Send(Row, true);
					Mark = GFrameCounter;
				})
				.Until(L(FString::Printf(TEXT("%s: held"), Row.Name)), [this, Row]()
				{
					if (Row.bAxis)
					{
						Send(Row, true); // axis events are per frame
					}
					const AStealthPawn* Pawn = Cast<AStealthPawn>(Presser()->GetPawn());
					const UPawnCommandComponent* Command = Pawn ? Pawn->GetPawnCommand() : nullptr;
					switch (Row.Expect)
					{
					case EExpect::Move: bObserved |= Command && Command->GetMoveIntent().Y > 0.5; break;
					case EExpect::Run: bObserved |= Presser()->GetCommandClient()->GetGait() == EGait::Run; break;
					case EExpect::WalkSlower: bObserved |= Presser()->GetCommandClient()->GetWalkSlowerPace() < 0.95f; break;
					case EExpect::Look: bObserved |= !Presser()->GetControlRotation().Equals(RotationBefore, 0.01); break;
					default: break;
					}
					return GFrameCounter >= Mark + HoldFrames;
				})
				.Then(L(FString::Printf(TEXT("%s: release"), Row.Name)), [this, Row]() { Send(Row, false); Mark = GFrameCounter; })
				.Until(L(FString::Printf(TEXT("%s: settle"), Row.Name)), [this]() { return GFrameCounter >= Mark + SettleFrames; })
				.Then(L(FString::Printf(TEXT("%s: check"), Row.Name)), [this, Row]()
				{
					const TArray<ECommandKind> Kinds = Log->KindsAt(EStealthCommandLogPoint::IngressOutput);
					const TArray<FOutgoingRpcSpy::FRecord> Pings = Spy->Excluding([](FName F) { return F != TEXT("ServerTeamPing"); });
					const FString Got = FString::JoinBy(Kinds, TEXT(","), [](ECommandKind K) { return FString::FromInt(static_cast<int32>(K)); });
					TestRunner->AddInfo(FString::Printf(TEXT("BINDING %s: commands [%s] pings %d toggles %d swaps %d observed %d"), Row.Name, *Got, Pings.Num(), Toggles, Swaps, bObserved ? 1 : 0));
					switch (Row.Expect)
					{
					case EExpect::Commands:
						if (Kinds != Row.Kinds) { Fail(FString::Printf(TEXT("%s: server command log [%s]"), Row.Name, *Got)); }
						break;
					case EExpect::Ping:
						if (Pings.Num() != 1 || Kinds.Num() != 0) { Fail(FString::Printf(TEXT("%s: %d ServerTeamPing, %d commands"), Row.Name, Pings.Num(), Kinds.Num())); }
						break;
					case EExpect::CameraToggle:
						if (Toggles != 1 || Kinds.Num() != 0) { Fail(FString::Printf(TEXT("%s: %d toggles, %d commands"), Row.Name, Toggles, Kinds.Num())); }
						break;
					case EExpect::ShoulderSwap:
						if (Swaps != 1 || Kinds.Num() != 0) { Fail(FString::Printf(TEXT("%s: %d swaps, %d commands"), Row.Name, Swaps, Kinds.Num())); }
						break;
					default:
						if (!bObserved || Kinds.Num() != 0) { Fail(FString::Printf(TEXT("%s: local effect %d, %d commands"), Row.Name, bObserved ? 1 : 0, Kinds.Num())); }
						break;
					}
				});
		}
		Network.Then(TEXT("every row reached its target"), [this]()
		{
			ASSERT_THAT(IsTrue(Failures.IsEmpty(), *FString::Join(Failures, TEXT("; "))));
		});
	}
};

#endif
