// Story iif-011 (production/epics/input-interaction-framework/story-011-spike-v5-commonui-menu-end.md): ADR-0011
// verification item V5 (and ADR-0021 UI-V2). A THROWAWAY probe harness, not a gate test (named under StealthSpike.,
// outside the per-commit ^StealthGame. filter); run on demand:
//   UnrealEditor-Cmd.exe StealthGame.uproject -ExecCmds="Automation RunTests StealthSpike.Input.V5; Quit" -unattended -nullrhi
// A standalone PIE on the fixture map with AStealthGameMode, so the local player has AStealthPlayerController and its
// production UPawnCommandClient (Common UI enabled, bEnableEnhancedInputSupport = true, UCommonGameViewportClient).
// Per run: the Prepare key is pressed (keyboard Q or Gamepad_LeftTrigger), a minimal UCommonActivatableWidget whose
// desired input config is Menu is activated, the key is released, the widget is deactivated. Recorded per run: the
// frame of UCommonUIActionRouterBase::OnActiveInputModeChanged(Menu), the frame of the builder's PrepareEnd, the frames
// of FlushPressedKeys (the router schedules it for the next tick), whether the key release still reached Enhanced
// Input (a terminal edge after the End is swallowed by the builder) and the PrepareEnd count. A same-frame variant
// presses and opens the menu in one frame. Pass rule (story): End in the same frame as the callback, strictly before
// the next tick's FlushPressedKeys, exactly one End in total, over 20 runs per key. Same-frame variant: no hold may start
// under the menu (no Begin and no End); the first run showed a Begin one frame after the push and its End only through
// the router's flush, which led to the builder's Menu-mode gate (UPawnCommandClient::bMenuMode). Results: Saved/Spikes/V5/v5-results.json.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Blueprint/UserWidget.h"
#include "CommonUITypes.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Input/CommonUIActionRouterBase.h"
#include "InputKeyEventArgs.h"
#include "LevelEditor.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Spikes/Input/StealthSpikeMenuWidget.h"
#include "StealthGameMode.h"
#include "StealthPlayerController.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealEdGlobals.h"
#include "View/Input/PawnCommandClient.h"

namespace SpikeV5
{
	constexpr int32 RunsPerKey = 20;
	constexpr int32 SettleFrames = 4;
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");

	struct FRun
	{
		FString Key;
		bool bSameFrame = false;
		int32 Begins = 0;
		int32 Ends = 0;
		uint64 PressFrame = 0;
		uint64 BeginFrame = 0;
		uint64 MenuFrame = 0;
		uint64 CallbackFrame = 0;
		uint64 EndFrame = 0;
		uint64 FlushFrame = 0;
		int32 SwallowedAfter = 0;
		/** The widget activation alone switched the router to Menu (else the router's SetActiveUIInputConfig did). */
		bool bWidgetSwitched = false;
		bool bPass = false;
	};

	struct FProbe
	{
		TWeakObjectPtr<AStealthPlayerController> PC;
		TWeakObjectPtr<UPawnCommandClient> Client;
		TWeakObjectPtr<UStealthSpikeMenuWidget> Menu;
		FRun Current;
		TArray<FRun> Results;
		int32 SwallowedAtStart = 0;
		int32 FlushMark = 0;
		bool bRouter = false;
		FString Error;

		bool TryInit()
		{
			UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
			AStealthPlayerController* Player = World ? Cast<AStealthPlayerController>(World->GetFirstPlayerController()) : nullptr;
			if (!Player || !Player->GetCommandClient() || !Player->GetLocalPlayer())
			{
				return false;
			}
			PC = Player;
			Client = Player->GetCommandClient();
			Client->OnOwnerCommandPredicted.AddLambda([this](ECommandKind Kind, FClientCommandSeq)
			{
				if (Kind == ECommandKind::PrepareBegin) { ++Current.Begins; Current.BeginFrame = GFrameCounter; }
				if (Kind == ECommandKind::PrepareEnd) { ++Current.Ends; Current.EndFrame = Current.EndFrame ? Current.EndFrame : GFrameCounter; }
			});
			if (UCommonUIActionRouterBase* Router = Player->GetLocalPlayer()->GetSubsystem<UCommonUIActionRouterBase>())
			{
				bRouter = true;
				Router->OnActiveInputModeChanged().AddLambda([this](ECommonInputMode Mode)
				{
					if (Mode == ECommonInputMode::Menu && !Current.CallbackFrame)
					{
						Current.CallbackFrame = GFrameCounter;
					}
				});
			}
			return true;
		}

		void Begin(const FString& Key, bool bSameFrame)
		{
			Current = FRun();
			Current.Key = Key;
			Current.bSameFrame = bSameFrame;
			FlushMark = AStealthPlayerController::GetFlushPressedKeysTrace().Num();
			SwallowedAtStart = Client.IsValid() ? Client->GetCore().GetSwallowedCount() : 0;
		}

		void Key(const FKey& InKey, EInputEvent Event)
		{
			if (APlayerController* Player = PC.Get())
			{
				Player->InputKey(FInputKeyEventArgs::CreateSimulated(InKey, Event, Event == IE_Pressed ? 1.f : 0.f));
				if (Event == IE_Pressed)
				{
					Current.PressFrame = GFrameCounter;
				}
			}
		}

		UCommonUIActionRouterBase* Router() const
		{
			const APlayerController* Player = PC.Get();
			return Player && Player->GetLocalPlayer() ? Player->GetLocalPlayer()->GetSubsystem<UCommonUIActionRouterBase>() : nullptr;
		}

		/**
		 * Pushes the menu: activates the Menu-config activatable widget, then (when the widget alone did not switch the
		 * router, as recorded in the first headless run) applies the same Menu input config through the router's own
		 * SetActiveUIInputConfig, which is the code path a widget activation ends in (ApplyUIInputConfig: the mode-change
		 * callback in the same frame, FlushPressedKeys scheduled for the next tick).
		 */
		void OpenMenu()
		{
			if (APlayerController* Player = PC.Get())
			{
				UStealthSpikeMenuWidget* Widget = CreateWidget<UStealthSpikeMenuWidget>(Player, UStealthSpikeMenuWidget::StaticClass());
				Widget->AddToViewport(100);
				Widget->ActivateWidget();
				Menu = Widget;
				Current.MenuFrame = GFrameCounter;
				UCommonUIActionRouterBase* R = Router();
				Current.bWidgetSwitched = R && R->GetActiveInputMode() == ECommonInputMode::Menu;
				if (R && !Current.bWidgetSwitched)
				{
					R->SetActiveUIInputConfig(FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture), Widget);
				}
			}
		}

		void CloseMenu()
		{
			if (UStealthSpikeMenuWidget* Widget = Menu.Get())
			{
				Widget->DeactivateWidget();
				Widget->RemoveFromParent();
			}
			Menu.Reset();
			if (UCommonUIActionRouterBase* R = Router())
			{
				R->SetActiveUIInputConfig(FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently));
			}
		}

		void Record()
		{
			const TArray<uint64>& Flushes = AStealthPlayerController::GetFlushPressedKeysTrace();
			for (int32 Index = FlushMark; Index < Flushes.Num(); ++Index)
			{
				if (Flushes[Index] >= Current.MenuFrame)
				{
					Current.FlushFrame = Flushes[Index];
					break;
				}
			}
			Current.SwallowedAfter = Client.IsValid() ? Client->GetCore().GetSwallowedCount() - SwallowedAtStart : 0;
			const bool bSynchronousEnd = Current.Begins == 1 && Current.Ends == 1 && Current.CallbackFrame != 0 && Current.EndFrame == Current.CallbackFrame
				&& (Current.FlushFrame == 0 || Current.EndFrame < Current.FlushFrame);
			// Same-frame variant: the press is processed in the frame after the menu push; the builder must not start a
			// hold under the menu at all (no Begin, no End), which is the ADR's "fresh press required" outcome.
			const bool bNoHoldUnderMenu = Current.bSameFrame && Current.Begins == 0 && Current.Ends == 0 && Current.CallbackFrame != 0;
			Current.bPass = bSynchronousEnd || bNoHoldUnderMenu;
			Results.Add(Current);
		}
	};

	struct FStep
	{
		TFunction<void()> Do;
		int32 WaitFrames = SettleFrames;
	};

	class FScriptCommand : public IAutomationLatentCommand
	{
	public:
		FScriptCommand(TSharedRef<FProbe> InProbe, TArray<FStep> InSteps) : Probe(InProbe), Steps(MoveTemp(InSteps)), StartTime(FPlatformTime::Seconds()) {}

		virtual bool Update() override
		{
			if (FPlatformTime::Seconds() - StartTime > 900.0)
			{
				Probe->Error = TEXT("timed out (900 s)");
				return true;
			}
			if (!bInitialised)
			{
				bInitialised = Probe->TryInit();
				Wait = 30;
				return false;
			}
			if (Wait > 0)
			{
				--Wait;
				return false;
			}
			if (!Probe->PC.IsValid())
			{
				Probe->Error = TEXT("the PIE player controller went away");
				return true;
			}
			if (Index >= Steps.Num())
			{
				return true;
			}
			Steps[Index].Do();
			Wait = Steps[Index].WaitFrames;
			++Index;
			return false;
		}

	private:
		TSharedRef<FProbe> Probe;
		TArray<FStep> Steps;
		double StartTime = 0.0;
		bool bInitialised = false;
		int32 Wait = 0;
		int32 Index = 0;
	};

	void StartStandalonePIE()
	{
		const FString File = FPackageName::LongPackageNameToFilename(FixtureMap, FPackageName::GetMapPackageExtension());
		FEditorFileUtils::LoadMap(File, false, false);
		ULevelEditorPlaySettings* PlaySettings = NewObject<ULevelEditorPlaySettings>();
		PlaySettings->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
		PlaySettings->SetPlayNumberOfClients(1);
		PlaySettings->bLaunchSeparateServer = false;
		PlaySettings->GameGetsMouseControl = false;
		PlaySettings->SetRunUnderOneProcess(true);
		FLevelEditorModule& LevelEditor = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		FRequestPlaySessionParams Session;
		Session.WorldType = EPlaySessionWorldType::PlayInEditor;
		Session.DestinationSlateViewport = LevelEditor.GetFirstActiveViewport();
		Session.EditorPlaySettings = PlaySettings;
		Session.GameModeOverride = AStealthGameMode::StaticClass();
		GUnrealEd->RequestPlaySession(Session);
		GUnrealEd->StartQueuedPlaySessionRequest();
	}

	TArray<FStep> BuildScript(const TSharedRef<FProbe>& P)
	{
		TArray<FStep> Steps;
		const TPair<FKey, FString> Keys[] = { { EKeys::Q, TEXT("Keyboard Q") }, { EKeys::Gamepad_LeftTrigger, TEXT("Gamepad LeftTrigger") } };
		for (const TPair<FKey, FString>& Key : Keys)
		{
			for (int32 SameFrame = 0; SameFrame < 2; ++SameFrame)
			{
				for (int32 Run = 0; Run < RunsPerKey; ++Run)
				{
					const FKey K = Key.Key;
					const FString Name = Key.Value;
					const bool bSame = SameFrame == 1;
					Steps.Add({ [P, Name, bSame]() { P->Begin(Name, bSame); }, 1 });
					if (bSame)
					{
						Steps.Add({ [P, K]() { P->Key(K, IE_Pressed); P->OpenMenu(); } });
					}
					else
					{
						Steps.Add({ [P, K]() { P->Key(K, IE_Pressed); } });
						Steps.Add({ [P]() { P->OpenMenu(); } });
					}
					Steps.Add({ [P, K]() { P->Key(K, IE_Released); } });
					Steps.Add({ [P]() { P->Record(); P->CloseMenu(); }, 8 });
				}
			}
		}
		return Steps;
	}

	void WriteResults(const FProbe& P, FAutomationTestBase& Test)
	{
		int32 Passes = 0;
		FString Json = FString::Printf(TEXT("{\n  \"engine\": \"%s\",\n  \"router\": %s,\n  \"callback\": \"UCommonUIActionRouterBase::OnActiveInputModeChanged() (DECLARE_EVENT_OneParam, ECommonInputMode)\",\n  \"runs\": [\n"),
			*FEngineVersion::Current().ToString(), P.bRouter ? TEXT("true") : TEXT("false"));
		for (int32 Index = 0; Index < P.Results.Num(); ++Index)
		{
			const FRun& R = P.Results[Index];
			Passes += R.bPass ? 1 : 0;
			Json += FString::Printf(TEXT("    {\"key\": \"%s\", \"sameFrame\": %s, \"begins\": %d, \"ends\": %d, \"pressFrame\": %llu, \"beginFrame\": %llu, \"menuFrame\": %llu, \"callbackFrame\": %llu, \"endFrame\": %llu, \"flushFrame\": %llu, \"swallowedTerminalEdges\": %d, \"widgetSwitchedRouter\": %s, \"pass\": %s}%s\n"),
				*R.Key, R.bSameFrame ? TEXT("true") : TEXT("false"), R.Begins, R.Ends, R.PressFrame, R.BeginFrame, R.MenuFrame, R.CallbackFrame, R.EndFrame, R.FlushFrame, R.SwallowedAfter, R.bWidgetSwitched ? TEXT("true") : TEXT("false"),
				R.bPass ? TEXT("true") : TEXT("false"), Index + 1 < P.Results.Num() ? TEXT(",") : TEXT(""));
			Test.AddInfo(FString::Printf(TEXT("V5RESULT %s | same-frame %d | begins %d ends %d | press %llu begin %llu menu %llu callback %llu end %llu flush %llu | swallowed %d | widget-switched %d | %s"),
				*R.Key, R.bSameFrame ? 1 : 0, R.Begins, R.Ends, R.PressFrame, R.BeginFrame, R.MenuFrame, R.CallbackFrame, R.EndFrame, R.FlushFrame, R.SwallowedAfter, R.bWidgetSwitched ? 1 : 0, R.bPass ? TEXT("PASS") : TEXT("FAIL")));
		}
		Json += FString::Printf(TEXT("  ],\n  \"passes\": %d,\n  \"total\": %d\n}\n"), Passes, P.Results.Num());
		FFileHelper::SaveStringToFile(Json, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Spikes/V5/v5-results.json")));
		Test.AddInfo(FString::Printf(TEXT("V5 %d/%d runs pass (router present: %d)"), Passes, P.Results.Num(), P.bRouter ? 1 : 0));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpikeV5CommonUiMenuEndTest, "StealthSpike.Input.V5CommonUiMenuEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSpikeV5CommonUiMenuEndTest::RunTest(const FString& Parameters)
{
	using namespace SpikeV5;
	TSharedRef<FProbe> Probe = MakeShared<FProbe>();
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([]() { StartStandalonePIE(); }, 0.f));
	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(Probe, BuildScript(Probe)));
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, Probe]()
	{
		if (!Probe->Error.IsEmpty())
		{
			AddError(FString::Printf(TEXT("V5 harness: %s"), *Probe->Error));
		}
		TestTrue(TEXT("V5 harness recorded runs"), Probe->Results.Num() > 0);
		WriteResults(*Probe, *this);
	}, 0.f));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
