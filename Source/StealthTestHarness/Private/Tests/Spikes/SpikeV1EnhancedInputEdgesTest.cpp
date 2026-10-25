// Story iif-008 (production/epics/input-interaction-framework/story-008-spike-v1-enhanced-input-edges.md):
// ADR-0011 verification item V1. A THROWAWAY probe harness, not a gate test: it is named under StealthSpike.,
// outside the per-commit ^StealthGame. filter, and is run on demand:
//   UnrealEditor-Cmd.exe StealthGame.uproject -ExecCmds="Automation RunTests StealthSpike.Input.V1; Quit"
//       -unattended -nullrhi -stdout
// It starts a standalone single-player PIE (AGameModeBase) on the editor map, clears the player's mapping contexts, adds this project's IMC_Common (priority 0)
// and IMC_Active (priority 1) from story iif-007, binds Started / Triggered / Completed / Canceled of IA_Interact
// (a Boolean action, E or Gamepad_FaceButton_Left) on a pushed input component, and runs each scenario N times per
// key with simulated key events (PlayerController::InputKey), counting the edges per press. A minimal builder probe
// mirrors ADR-0011 section 9 (Begin on Started; End on the first Completed/Canceled or on a flush; later terminal
// edges for the same press are swallowed). Results are logged as "V1RESULT" lines and written to
// Saved/Spikes/V1/v1-results.json for the report in production/qa/spikes/. Scenario outcomes are findings for the
// report, not test errors: the test fails only when the harness itself cannot run.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "GameFramework/GameModeBase.h"
#include "LevelEditor.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UnrealEdGlobals.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

namespace SpikeV1
{
	/** Repetitions per scenario and key (story iif-008: N = 20). */
	constexpr int32 RunsPerScenario = 20;
	/** Frames to let the input pipeline settle after each step. */
	constexpr int32 SettleFrames = 3;

	/** Edge counts for one press of one scenario. */
	struct FRun
	{
		FString Scenario;
		FString Key;
		int32 Started = 0;
		int32 Triggered = 0;
		int32 Completed = 0;
		int32 Canceled = 0;
		int32 Begins = 0;
		int32 Ends = 0;
		int32 Swallowed = 0;
	};

	/** Shared state of the probe: the player, the contexts, the counters and the builder probe. */
	struct FProbe
	{
		TWeakObjectPtr<APlayerController> PC;
		TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> Subsystem;
		TStrongObjectPtr<UInputAction> Interact;
		TStrongObjectPtr<UInputMappingContext> Common;
		TStrongObjectPtr<UInputMappingContext> Active;
		TStrongObjectPtr<UInputMappingContext> Informant;
		TStrongObjectPtr<UInputMappingContext> Menu;
		TStrongObjectPtr<UInputAction> MenuBack;
		FRun Current;
		bool bOutstanding = false;
		TArray<FRun> Results;
		FString Error;

		void Reset(const FString& Scenario, const FString& Key)
		{
			Current = FRun();
			Current.Scenario = Scenario;
			Current.Key = Key;
			bOutstanding = false;
		}

		void OnStarted()
		{
			++Current.Started;
			if (!bOutstanding)
			{
				bOutstanding = true;
				++Current.Begins;
			}
		}

		void OnTerminal(bool bCanceled)
		{
			++(bCanceled ? Current.Canceled : Current.Completed);
			if (bOutstanding)
			{
				bOutstanding = false;
				++Current.Ends;
			}
			else
			{
				++Current.Swallowed;
			}
		}

		/** The builder's outstanding-Begin flush (ADR-0011 section 9): End every outstanding Begin now. */
		void FlushOutstanding()
		{
			if (bOutstanding)
			{
				bOutstanding = false;
				++Current.Ends;
			}
		}

		void Key(const FKey& InKey, EInputEvent Event) const
		{
			if (APlayerController* Player = PC.Get())
			{
				Player->InputKey(FInputKeyEventArgs::CreateSimulated(InKey, Event, Event == IE_Pressed ? 1.f : 0.f));
			}
		}

		void Add(UInputMappingContext* Context, int32 Priority, bool bIgnoreHeld) const
		{
			FModifyContextOptions Options;
			Options.bIgnoreAllPressedKeysUntilRelease = bIgnoreHeld;
			if (UEnhancedInputLocalPlayerSubsystem* S = Subsystem.Get())
			{
				S->AddMappingContext(Context, Priority, Options);
			}
		}

		void Remove(UInputMappingContext* Context, bool bIgnoreHeld) const
		{
			FModifyContextOptions Options;
			Options.bIgnoreAllPressedKeysUntilRelease = bIgnoreHeld;
			if (UEnhancedInputLocalPlayerSubsystem* S = Subsystem.Get())
			{
				S->RemoveMappingContext(Context, Options);
			}
		}

		/** Finds the PIE player, swaps in the project contexts and binds the counters. False until PIE is up. */
		bool TryInit()
		{
			UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			ULocalPlayer* Local = Player ? Player->GetLocalPlayer() : nullptr;
			UEnhancedInputLocalPlayerSubsystem* S = Local ? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
			if (!S)
			{
				return false;
			}
			PC = Player;
			Subsystem = S;
			S->ClearAllMappings();
			Add(Common.Get(), 0, true);
			Add(Active.Get(), 1, true);

			UEnhancedInputComponent* Input = NewObject<UEnhancedInputComponent>(Player, TEXT("SpikeV1Input"));
			Input->RegisterComponent();
			Input->BindActionInstanceLambda(Interact.Get(), ETriggerEvent::Started, [this](const FInputActionInstance&) { OnStarted(); });
			Input->BindActionInstanceLambda(Interact.Get(), ETriggerEvent::Triggered, [this](const FInputActionInstance&) { ++Current.Triggered; });
			Input->BindActionInstanceLambda(Interact.Get(), ETriggerEvent::Completed, [this](const FInputActionInstance&) { OnTerminal(false); });
			Input->BindActionInstanceLambda(Interact.Get(), ETriggerEvent::Canceled, [this](const FInputActionInstance&) { OnTerminal(true); });
			Player->PushInputComponent(Input);
			return true;
		}
	};

	/** One step of the frame script: an action, then a number of frames to wait. */
	struct FStep
	{
		TFunction<void()> Do;
		int32 WaitFrames = SettleFrames;
	};

	/** Runs the frame script against the PIE player, one step per frame after its wait. */
	class FScriptCommand : public IAutomationLatentCommand
	{
	public:
		FScriptCommand(TSharedRef<FProbe> InProbe, TArray<FStep> InSteps)
			: Probe(InProbe), Steps(MoveTemp(InSteps)), StartTime(FPlatformTime::Seconds())
		{
		}

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
				Wait = 10;
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

	/**
	 * Starts a single-player standalone PIE session in the level viewport with the engine's AGameModeBase, so neither a
	 * second client window (unavailable under -nullrhi) nor the prototype game mode's pawn bindings interfere. Mirrors
	 * CQTest's FBasePIENetworkComponent::StartPie.
	 */
	void StartStandalonePIE()
	{
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
		Session.GameModeOverride = AGameModeBase::StaticClass();
		GUnrealEd->RequestPlaySession(Session);
		GUnrealEd->StartQueuedPlaySessionRequest();
	}

	/** Builds the step list of every scenario, for both keys, N runs each. */
	TArray<FStep> BuildScript(const TSharedRef<FProbe>& P)
	{
		TArray<FStep> Steps;
		const TPair<FKey, FString> Keys[] = { { EKeys::E, TEXT("Keyboard E") }, { EKeys::Gamepad_FaceButton_Left, TEXT("Gamepad FaceButton_Left") } };

		using FBody = TFunction<void(TArray<FStep>&, const FKey&)>;
		auto Record = [P]() { P->Results.Add(P->Current); };
		auto Press = [P](FKey K) { return FStep{ [P, K]() { P->Key(K, IE_Pressed); } }; };
		auto Release = [P](FKey K) { return FStep{ [P, K]() { P->Key(K, IE_Released); } }; };

		const TPair<FString, FBody> Scenarios[] = {
			{ TEXT("S1 plain press and release"), [&](TArray<FStep>& S, const FKey& K) {
				S.Add(Press(K)); S.Add(Release(K)); } },
			{ TEXT("S2 context add while held (IMC_Informant, ignore-held true)"), [&](TArray<FStep>& S, const FKey& K) {
				S.Add(Press(K));
				S.Add({ [P]() { P->Add(P->Informant.Get(), 5, true); } });
				S.Add(Release(K));
				S.Add({ [P, Record]() { Record(); P->Remove(P->Informant.Get(), true); P->Current = FRun(); } }); } },
			{ TEXT("S3a context remove while held (IMC_Active, ignore-held true)"), [&](TArray<FStep>& S, const FKey& K) {
				S.Add(Press(K));
				S.Add({ [P]() { P->Remove(P->Active.Get(), true); } });
				S.Add(Release(K));
				S.Add({ [P, Record]() { Record(); P->Add(P->Active.Get(), 1, true); P->Current = FRun(); } }); } },
			{ TEXT("S3b context remove while held (IMC_Active, ignore-held false)"), [&](TArray<FStep>& S, const FKey& K) {
				S.Add(Press(K));
				S.Add({ [P]() { P->Remove(P->Active.Get(), false); } });
				S.Add(Release(K));
				S.Add({ [P, Record]() { Record(); P->Add(P->Active.Get(), 1, true); P->Current = FRun(); } }); } },
			{ TEXT("S3c held across remove and re-add of IMC_Active (ignore-held true)"), [&](TArray<FStep>& S, const FKey& K) {
				S.Add(Press(K));
				S.Add({ [P]() { P->Remove(P->Active.Get(), true); } });
				S.Add({ [P]() { P->Add(P->Active.Get(), 1, true); } });
				S.Add(Release(K)); } },
			{ TEXT("S3d held across remove and re-add of IMC_Active (ignore-held false)"), [&](TArray<FStep>& S, const FKey& K) {
				S.Add(Press(K));
				S.Add({ [P]() { P->Remove(P->Active.Get(), false); } });
				S.Add({ [P]() { P->Add(P->Active.Get(), 1, false); } });
				S.Add(Release(K)); } },
			{ TEXT("S5 menu open (IMC_Menu at priority 100 + FlushPressedKeys)"), [&](TArray<FStep>& S, const FKey& K) {
				S.Add(Press(K));
				S.Add({ [P]() { P->Add(P->Menu.Get(), 100, true); if (APlayerController* PC = P->PC.Get()) { PC->FlushPressedKeys(); } } });
				S.Add(Release(K));
				S.Add({ [P, Record]() { Record(); P->Remove(P->Menu.Get(), true); P->Current = FRun(); } }); } },
			{ TEXT("S6a builder flush, then swap Active->Informant, release, swap back"), [&](TArray<FStep>& S, const FKey& K) {
				S.Add(Press(K));
				S.Add({ [P]() { P->FlushOutstanding(); P->Remove(P->Active.Get(), true); P->Add(P->Informant.Get(), 1, true); } });
				S.Add(Release(K));
				S.Add({ [P]() { P->Remove(P->Informant.Get(), true); P->Add(P->Active.Get(), 1, true); } }); } },
			{ TEXT("S6b builder flush, swap Active->Informant->Active while held, release"), [&](TArray<FStep>& S, const FKey& K) {
				S.Add(Press(K));
				S.Add({ [P]() { P->FlushOutstanding(); P->Remove(P->Active.Get(), true); P->Add(P->Informant.Get(), 1, true); } });
				S.Add({ [P]() { P->Remove(P->Informant.Get(), true); P->Add(P->Active.Get(), 1, true); } });
				S.Add(Release(K)); } },
		};

		for (const TPair<FString, FBody>& Scenario : Scenarios)
		{
			for (const TPair<FKey, FString>& Key : Keys)
			{
				for (int32 Run = 0; Run < RunsPerScenario; ++Run)
				{
					const FString Name = Scenario.Key;
					const FString KeyName = Key.Value;
					Steps.Add({ [P, Name, KeyName]() { P->Reset(Name, KeyName); }, 1 });
					Scenario.Value(Steps, Key.Key);
					// Scenarios that record inside their body end with an empty Current; the rest record here.
					Steps.Add({ [P, Record]() { if (!P->Current.Scenario.IsEmpty()) { Record(); } }, 1 });
				}
			}
		}
		return Steps;
	}

	/** Writes the per-run results, the build and the CVar to Saved/Spikes/V1/v1-results.json and the log. */
	void WriteResults(const FProbe& P, FAutomationTestBase& Test)
	{
		const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush"));
		const FString CVarValue = CVar ? CVar->GetString() : FString(TEXT("<not found>"));
		FString Json = FString::Printf(TEXT("{\n  \"engine\": \"%s\",\n  \"cvar_EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush\": \"%s\",\n  \"runs_per_scenario_and_key\": %d,\n  \"runs\": [\n"),
			*FEngineVersion::Current().ToString(), *CVarValue, RunsPerScenario);
		for (int32 Index = 0; Index < P.Results.Num(); ++Index)
		{
			const FRun& R = P.Results[Index];
			Json += FString::Printf(TEXT("    {\"scenario\": \"%s\", \"key\": \"%s\", \"started\": %d, \"triggered\": %d, \"completed\": %d, \"canceled\": %d, \"begins\": %d, \"ends\": %d, \"swallowed\": %d}%s\n"),
				*R.Scenario, *R.Key, R.Started, R.Triggered, R.Completed, R.Canceled, R.Begins, R.Ends, R.Swallowed, Index + 1 < P.Results.Num() ? TEXT(",") : TEXT(""));
			Test.AddInfo(FString::Printf(TEXT("V1RESULT %s | %s | started=%d completed=%d canceled=%d begins=%d ends=%d swallowed=%d"),
				*R.Scenario, *R.Key, R.Started, R.Completed, R.Canceled, R.Begins, R.Ends, R.Swallowed));
		}
		Json += TEXT("  ]\n}\n");
		const FString File = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Spikes/V1/v1-results.json"));
		FFileHelper::SaveStringToFile(Json, *File);
		Test.AddInfo(FString::Printf(TEXT("V1 engine %s, CVar EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush=%s, %d runs, results %s"),
			*FEngineVersion::Current().ToString(), *CVarValue, P.Results.Num(), *File));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpikeV1EnhancedInputEdgesTest, "StealthSpike.Input.V1EnhancedInputEdges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSpikeV1EnhancedInputEdgesTest::RunTest(const FString& Parameters)
{
	using namespace SpikeV1;
	TSharedRef<FProbe> Probe = MakeShared<FProbe>();
	auto LoadAsset = [](const TCHAR* Name) { return FString::Printf(TEXT("/Game/StealthGame/Input/%s.%s"), Name, Name); };
	Probe->Interact.Reset(LoadObject<UInputAction>(nullptr, *LoadAsset(TEXT("IA_Interact"))));
	Probe->Common.Reset(LoadObject<UInputMappingContext>(nullptr, *LoadAsset(TEXT("IMC_Common"))));
	Probe->Active.Reset(LoadObject<UInputMappingContext>(nullptr, *LoadAsset(TEXT("IMC_Active"))));
	Probe->Informant.Reset(LoadObject<UInputMappingContext>(nullptr, *LoadAsset(TEXT("IMC_Informant"))));
	if (!TestTrue(TEXT("The story iif-007 input assets load"), Probe->Interact && Probe->Common && Probe->Active && Probe->Informant))
	{
		return false;
	}
	// A stand-in for ADR-0021's IMC_Menu (UI navigation only; the real Common UI menu path is spike V5, story iif-011).
	Probe->MenuBack.Reset(NewObject<UInputAction>(GetTransientPackage(), TEXT("IA_SpikeV1MenuBack")));
	Probe->Menu.Reset(NewObject<UInputMappingContext>(GetTransientPackage(), TEXT("IMC_SpikeV1Menu")));
	Probe->Menu->MapKey(Probe->MenuBack.Get(), EKeys::Escape);

	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([]() { StartStandalonePIE(); }, 0.f));
	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(Probe, BuildScript(Probe)));
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, Probe]()
	{
		if (!Probe->Error.IsEmpty())
		{
			AddError(FString::Printf(TEXT("V1 harness: %s"), *Probe->Error));
		}
		TestTrue(TEXT("V1 harness recorded runs"), Probe->Results.Num() > 0);
		WriteResults(*Probe, *this);
	}, 0.f));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
