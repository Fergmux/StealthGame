// Story iif-019 (production/epics/input-interaction-framework/story-019-remove-sample-ctrl-jog-toggle.md), decision D37:
// GIVEN a PIE character walking with W held WHEN Left Ctrl is pressed and released THEN the character's gait is not
// toggled between walk and jog (its Gait value and speed are unchanged before and after the tap), and Ctrl held with W
// gives the 0.8 pace.
// Pinned observables (qa-lead): the reparented sample Blueprint's gait variable, found by read-only inspection, is the
// "Gait" property (E_Gait) of CBP_SandboxCharacter; the effective (applied) Enhanced Input mappings of the player
// contain exactly one mapping for LeftControl, to IA_WalkSlower (the production IMC_Common at a priority above the
// sample's IMC_Sandbox consumes the key, so the sample's IA_Walk toggle never receives it); the 0.8 pace is asserted as a
// ratio of the walk speed (112 cm/s needs SCL's WalkSpeedCmPerSec 140, story SCL-010). No sample asset is edited.
// CQTest: NETWORK_TEST_CLASS(NoSampleCtrlToggle, "StealthGame.Input.PIE"), listen server + 1 client.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "Server/Core/StealthPawnCommandComponent.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "StealthGameMode.h"
#include "View/Core/StealthPawn.h"
#include "View/Core/WalkSlowerScale.h"

namespace NoSampleCtrlToggleTestData
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	const TCHAR* const NetworkTimeoutCeilingSeconds = TEXT("240");
	constexpr uint64 SettleFrames = 150;
	constexpr uint64 SampleFrames = 45;
	constexpr double PaceRatioTolerance = 0.05;
	constexpr double SpeedUnchangedTolerance = 0.05;
	/** The reparented sample Blueprint's gait variable (read-only inspection of CBP_SandboxCharacter). */
	const TCHAR* const SampleGaitProperty = TEXT("Gait");
	constexpr double CircleYawPerFrameDeg = 0.6;
}

NETWORK_TEST_CLASS(NoSampleCtrlToggle, "StealthGame.Input.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;

	uint64 Mark = 0;
	double SpeedSum = 0.0;
	int32 SpeedCount = 0;
	double SpeedBefore = 0.0;
	double SpeedAfter = 0.0;
	double SpeedCtrl = 0.0;
	int64 GaitBefore = -1;
	int64 GaitAfter = -1;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(NoSampleCtrlToggleTestData::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
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
		Network.StartOnFixtureMap(NoSampleCtrlToggleTestData::FixtureMap);
	}

	static int64 ReadGait(const APawn* Pawn)
	{
		const FProperty* Property = Pawn ? Pawn->GetClass()->FindPropertyByName(NoSampleCtrlToggleTestData::SampleGaitProperty) : nullptr;
		if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
		{
			return EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(EnumProp->ContainerPtrToValuePtr<void>(Pawn));
		}
		if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
		{
			return ByteProp->GetPropertyValue_InContainer(Pawn);
		}
		return -1;
	}

	void Key(FBasePIENetworkComponentState& Client, const FKey& InKey, bool bDown)
	{
		if (APlayerController* PC = Client.World->GetFirstPlayerController())
		{
			PC->InputKey(FInputKeyEventArgs::CreateSimulated(InKey, bDown ? IE_Pressed : IE_Released, bDown ? 1.f : 0.f));
		}
	}

	/** Samples the client pawn's 2D speed over the last SampleFrames of a SettleFrames window. */
	void QueueMeasure(const TCHAR* Label, double& Out)
	{
		using namespace NoSampleCtrlToggleTestData;
		Network
			.ThenClients(*FString::Printf(TEXT("%s: start"), Label), [this](FBasePIENetworkComponentState&) { Mark = GFrameCounter; SpeedSum = 0.0; SpeedCount = 0; })
			.UntilClients(*FString::Printf(TEXT("%s: measure"), Label), [this, &Out](FBasePIENetworkComponentState& Client)
			{
				// Walk a slow circle (~3 m radius) so the walk stays in the open street, whatever the spawn point.
				APlayerController* Walker = Client.World->GetFirstPlayerController();
				Walker->SetControlRotation(Walker->GetControlRotation() + FRotator(0.0, CircleYawPerFrameDeg, 0.0));
				const APawn* Pawn = Walker->GetPawn();
				if (const ACharacter* Character = Cast<ACharacter>(Pawn); Character && GFrameCounter == Mark + SettleFrames - SampleFrames)
				{
					const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
					const AStealthPawn* Stealth = Cast<AStealthPawn>(Pawn);
					TestRunner->AddInfo(FString::Printf(TEXT("DIAG mode %d maxspeed %.1f accel %s lastinput %s ignored %d intent %s role %d"),
						static_cast<int32>(Move->MovementMode), Move->GetMaxSpeed(), *Move->GetCurrentAcceleration().ToString(), *Move->GetLastInputVector().ToString(),
						Character->IsMoveInputIgnored() ? 1 : 0, Stealth && Stealth->GetPawnCommand() ? *Stealth->GetPawnCommand()->GetMoveIntent().ToString() : TEXT("-"),
						static_cast<int32>(Character->GetLocalRole())));
				}
				if (Pawn && GFrameCounter > Mark + SettleFrames - SampleFrames)
				{
					SpeedSum += Pawn->GetVelocity().Size2D();
					++SpeedCount;
				}
				if (GFrameCounter >= Mark + SettleFrames)
				{
					Out = SpeedCount ? SpeedSum / SpeedCount : 0.0;
					return true;
				}
				return false;
			});
	}

	TEST_METHOD(CtrlTapLeavesGaitAndSpeedAndCtrlHoldGivesTheSlowPace)
	{
		using namespace NoSampleCtrlToggleTestData;
		Network
			.UntilClients(TEXT("the client possesses its pawn"), [](FBasePIENetworkComponentState& Client)
			{
				const APlayerController* PC = Client.World ? Client.World->GetFirstPlayerController() : nullptr;
				return PC && Cast<AStealthPawn>(PC->GetPawn()) != nullptr;
			})
			.ThenClients(TEXT("exactly one effective LeftControl mapping, to IA_WalkSlower"), [this](FBasePIENetworkComponentState& Client)
			{
				const ULocalPlayer* Local = Client.World->GetFirstPlayerController()->GetLocalPlayer();
				const UEnhancedInputLocalPlayerSubsystem* Subsystem = Local ? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
				ASSERT_THAT(IsNotNull(Subsystem, TEXT("Enhanced Input subsystem")));
				// QueryKeysMappedToAction reads the APPLIED mappings (after priority and key consumption).
				const UInputAction* WalkSlower = LoadObject<UInputAction>(nullptr, TEXT("/Game/StealthGame/Input/IA_WalkSlower.IA_WalkSlower"));
				const UInputAction* SampleWalk = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/IA_Walk.IA_Walk"));
				ASSERT_THAT(IsTrue(WalkSlower && SampleWalk, TEXT("IA_WalkSlower and the sample's IA_Walk load (read only)")));
				const TArray<FKey> SlowerKeys = Subsystem->QueryKeysMappedToAction(WalkSlower);
				const TArray<FKey> SampleKeys = Subsystem->QueryKeysMappedToAction(SampleWalk);
				TestRunner->AddInfo(FString::Printf(TEXT("applied keys: IA_WalkSlower [%s], sample IA_Walk [%s]"),
					*FString::JoinBy(SlowerKeys, TEXT(","), [](const FKey& K) { return K.ToString(); }), *FString::JoinBy(SampleKeys, TEXT(","), [](const FKey& K) { return K.ToString(); })));
				ASSERT_THAT(IsTrue(SlowerKeys.Contains(EKeys::LeftControl), TEXT("LeftControl is applied to IA_WalkSlower")));
				ASSERT_THAT(IsFalse(SampleKeys.Contains(EKeys::LeftControl), TEXT("LeftControl is not applied to the sample's IA_Walk jog toggle")));
				ASSERT_THAT(IsTrue(ReadGait(Client.World->GetFirstPlayerController()->GetPawn()) >= 0, TEXT("the sample Gait property is readable")));
				Key(Client, EKeys::W, true);
			});
		QueueMeasure(TEXT("walking before the tap"), SpeedBefore);
		Network
			.ThenClients(TEXT("record the gait; tap Left Ctrl"), [this](FBasePIENetworkComponentState& Client)
			{
				GaitBefore = ReadGait(Client.World->GetFirstPlayerController()->GetPawn());
				Key(Client, EKeys::LeftControl, true);
				Mark = GFrameCounter;
			})
			.UntilClients(TEXT("tap held for a few frames"), [this](FBasePIENetworkComponentState&) { return GFrameCounter > Mark + 6; })
			.ThenClients(TEXT("release Left Ctrl"), [this](FBasePIENetworkComponentState& Client) { Key(Client, EKeys::LeftControl, false); });
		QueueMeasure(TEXT("walking after the tap"), SpeedAfter);
		Network
			.ThenClients(TEXT("gait after the tap; hold Left Ctrl"), [this](FBasePIENetworkComponentState& Client)
			{
				GaitAfter = ReadGait(Client.World->GetFirstPlayerController()->GetPawn());
				Key(Client, EKeys::LeftControl, true);
			});
		QueueMeasure(TEXT("walking with Left Ctrl held"), SpeedCtrl);
		Network.ThenClients(TEXT("assert"), [this](FBasePIENetworkComponentState& Client)
		{
			Key(Client, EKeys::LeftControl, false);
			Key(Client, EKeys::W, false);
			TestRunner->AddInfo(FString::Printf(TEXT("NOSAMPLECTRL gait before %lld after %lld; speed before %.1f after %.1f ctrl %.1f (ratio %.3f)"),
				GaitBefore, GaitAfter, SpeedBefore, SpeedAfter, SpeedCtrl, SpeedBefore > 0.0 ? SpeedCtrl / SpeedBefore : 0.0));
			ASSERT_THAT(IsTrue(SpeedBefore > 10.0, TEXT("the character walked")));
			ASSERT_THAT(AreEqual(GaitBefore, GaitAfter, TEXT("the sample Gait is unchanged by the Ctrl tap")));
			ASSERT_THAT(IsNear(1.0, SpeedAfter / SpeedBefore, SpeedUnchangedTolerance, TEXT("the speed is unchanged by the Ctrl tap (no jog toggle)")));
			ASSERT_THAT(IsNear(static_cast<double>(StealthPaceSet::Slow), SpeedCtrl / SpeedBefore, PaceRatioTolerance, TEXT("Ctrl held with W gives the 0.8 pace")));
		});
	}
};

#endif
