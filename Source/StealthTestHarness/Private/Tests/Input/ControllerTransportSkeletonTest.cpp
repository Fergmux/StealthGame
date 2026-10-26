// Story iif-094 (production/epics/input-interaction-framework/story-094-controller-transport-skeleton.md), ADR-0011
// Key Interfaces (controller transport) and sections 2 and 9:
//   GIVEN AStealthPlayerController and a possessed AStealthPawn, WHEN SetupInputComponent runs and the class is reflected,
//   THEN it binds the IA_* actions through UEnhancedInputComponent; ServerPawnCommand(FNetPawnCommand) is a reliable
//   WithValidation Server RPC; ClientCommandReplies and ClientSetPossessionEpoch are reliable Client RPCs;
//   UPawnCommandClient exists as a client-only component tracking outstanding Begins; and no gameplay RPC exists on
//   AStealthPawn or its components.
// Two parts: reflection (no world) and a 1-client listen-server PIE on the fixture map (CQTest
// NETWORK_TEST_CLASS(ControllerTransportSkeleton, "StealthGame.Input.PIE")). The GDD-style name
// StealthGame.Input.ControllerTransportSkeleton maps to the reflection test below.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Server/Core/StealthPawnCommandComponent.h"
#include "StealthPlayerController.h"
#include "View/Input/PawnCommandClient.h"
#include "View/Core/StealthPawn.h"

namespace ControllerTransportSkeletonTestData
{
	/** The actions the controller binds (ADR-0011 section 9; IA_Menu belongs to the menu stack, ADR-0021). */
	const TCHAR* const BoundActions[] = {
		TEXT("IA_Move"), TEXT("IA_Look"), TEXT("IA_Run"), TEXT("IA_Traverse"), TEXT("IA_Interact"), TEXT("IA_Prepare"), TEXT("IA_Attack"), TEXT("IA_Hide"),
		TEXT("IA_CameraToggle"), TEXT("IA_ShoulderSwap"), TEXT("IA_WalkSlower"), TEXT("IA_Ping"),
	};

	bool HasFlags(const UClass* Class, FName Function, EFunctionFlags Flags)
	{
		const UFunction* Found = Class->FindFunctionByName(Function);
		return Found && Found->HasAllFunctionFlags(Flags);
	}

	/** Every Net function declared on Class itself (engine movement RPCs of the parents are not gameplay RPCs). */
	TArray<FString> DeclaredRpcs(const UClass* Class)
	{
		TArray<FString> Out;
		for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if (It->HasAnyFunctionFlags(FUNC_Net))
			{
				Out.Add(FString::Printf(TEXT("%s::%s"), *Class->GetName(), *It->GetName()));
			}
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputControllerTransportSkeletonTest, "StealthGame.Input.ControllerTransportSkeleton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInputControllerTransportSkeletonTest::RunTest(const FString& Parameters)
{
	using namespace ControllerTransportSkeletonTestData;
	const UClass* Controller = AStealthPlayerController::StaticClass();

	TestTrue(TEXT("ServerPawnCommand is a reliable WithValidation Server RPC"),
		HasFlags(Controller, TEXT("ServerPawnCommand"), FUNC_Net | FUNC_NetServer | FUNC_NetReliable | FUNC_NetValidate));
	TestTrue(TEXT("ClientCommandReplies is a reliable Client RPC"), HasFlags(Controller, TEXT("ClientCommandReplies"), FUNC_Net | FUNC_NetClient | FUNC_NetReliable));
	TestTrue(TEXT("ClientSetPossessionEpoch is a reliable Client RPC"), HasFlags(Controller, TEXT("ClientSetPossessionEpoch"), FUNC_Net | FUNC_NetClient | FUNC_NetReliable));
	TestTrue(TEXT("ServerTeamPing (IA_Ping) is a separate Server RPC, never ServerPawnCommand"), HasFlags(Controller, TEXT("ServerTeamPing"), FUNC_Net | FUNC_NetServer));

	// The RPC parameters carry the command and reply types (the client never names the pawn).
	if (const UFunction* Server = Controller->FindFunctionByName(TEXT("ServerPawnCommand")))
	{
		int32 Params = 0;
		bool bIsCommand = false;
		for (TFieldIterator<FProperty> It(Server); It; ++It)
		{
			++Params;
			const FStructProperty* Struct = CastField<FStructProperty>(*It);
			bIsCommand |= Struct && Struct->Struct == FNetPawnCommand::StaticStruct();
		}
		TestTrue(TEXT("ServerPawnCommand takes exactly one FNetPawnCommand"), Params == 1 && bIsCommand);
	}

	// UPawnCommandClient: a default subobject of the controller, non-replicated, with the outstanding-Begin table.
	TArray<UObject*> Subobjects;
	GetMutableDefault<AStealthPlayerController>()->GetDefaultSubobjects(Subobjects);
	const UPawnCommandClient* Client = nullptr;
	for (UObject* Subobject : Subobjects)
	{
		Client = Client ? Client : Cast<UPawnCommandClient>(Subobject);
	}
	if (TestNotNull(TEXT("the controller carries a UPawnCommandClient"), Client))
	{
		TestFalse(TEXT("UPawnCommandClient does not replicate"), Client->GetIsReplicated());
		TestFalse(TEXT("no Interact Begin is outstanding at rest"), Client->GetCore().IsOutstanding(FPawnCommandClientCore::EHeld::Interact));
		TestFalse(TEXT("no Prepare Begin is outstanding at rest"), Client->GetCore().IsOutstanding(FPawnCommandClientCore::EHeld::Prepare));
	}

	// No gameplay RPC on AStealthPawn or its components.
	TArray<FString> Rpcs = DeclaredRpcs(AStealthPawn::StaticClass());
	TArray<UObject*> PawnSubobjects;
	GetMutableDefault<AStealthPawn>()->GetDefaultSubobjects(PawnSubobjects);
	for (UObject* Subobject : PawnSubobjects)
	{
		if (Subobject->IsA<UActorComponent>() && Subobject->GetClass()->GetOutermost() != GetTransientPackage())
		{
			// Only the project's own component classes (engine CMC parents carry the engine's movement RPCs).
			if (Subobject->GetClass()->GetPathName().StartsWith(TEXT("/Script/Stealth")))
			{
				Rpcs.Append(DeclaredRpcs(Subobject->GetClass()));
			}
		}
	}
	TestTrue(FString::Printf(TEXT("no RPC is declared on AStealthPawn or its components (%s)"), *FString::Join(Rpcs, TEXT(", "))), Rpcs.IsEmpty());
	return true;
}

#if defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "StealthGameMode.h"

namespace ControllerTransportSkeletonPieData
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	const TCHAR* const NetworkTimeoutCeilingSeconds = TEXT("240");
}

NETWORK_TEST_CLASS(ControllerTransportSkeleton, "StealthGame.Input.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(ControllerTransportSkeletonPieData::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
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
		Network.StartOnFixtureMap(ControllerTransportSkeletonPieData::FixtureMap);
	}

	TEST_METHOD(ClientBindsTheGameplayActionsOnTheController)
	{
		using namespace ControllerTransportSkeletonTestData;
		Network
			.UntilClients(TEXT("the client possesses its pawn"), [](FBasePIENetworkComponentState& Client)
			{
				const APlayerController* PC = Client.World ? Client.World->GetFirstPlayerController() : nullptr;
				return PC && Cast<AStealthPawn>(PC->GetPawn()) != nullptr && PC->InputComponent != nullptr;
			})
			.ThenClients(TEXT("Enhanced Input bindings, contexts and the epoch"), [this](FBasePIENetworkComponentState& Client)
			{
				AStealthPlayerController* PC = Cast<AStealthPlayerController>(Client.World->GetFirstPlayerController());
				ASSERT_THAT(IsNotNull(PC, TEXT("the client controller is AStealthPlayerController")));
				const UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PC->InputComponent);
				ASSERT_THAT(IsNotNull(Input, TEXT("SetupInputComponent made an UEnhancedInputComponent")));
				for (const TCHAR* Name : BoundActions)
				{
					bool bBound = false;
					for (const TUniquePtr<FEnhancedInputActionEventBinding>& Binding : Input->GetActionEventBindings())
					{
						bBound |= Binding.IsValid() && Binding->GetAction() && Binding->GetAction()->GetName() == Name;
					}
					ASSERT_THAT(IsTrue(bBound, *FString::Printf(TEXT("%s is bound on the controller's input component"), Name)));
				}
				const UEnhancedInputLocalPlayerSubsystem* Subsystem = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
				ASSERT_THAT(IsNotNull(Subsystem, TEXT("Enhanced Input local player subsystem")));
				const UInputMappingContext* Common = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/StealthGame/Input/IMC_Common.IMC_Common"));
				const UInputMappingContext* Active = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/StealthGame/Input/IMC_Active.IMC_Active"));
				ASSERT_THAT(IsTrue(Common && Subsystem->HasMappingContext(Common), TEXT("IMC_Common is applied")));
				ASSERT_THAT(IsTrue(Active && Subsystem->HasMappingContext(Active), TEXT("IMC_Active is applied")));
				ASSERT_THAT(IsTrue(PC->GetOwnerPossessionEpoch() >= 1, TEXT("ClientSetPossessionEpoch delivered an epoch")));
				ASSERT_THAT(IsTrue(PC->GetCommandClient() != nullptr, TEXT("the client-only command builder exists")));
			})
			.ThenServer(TEXT("the server epoch and the reply sink"), [this](FBasePIENetworkComponentState& Server)
			{
				int32 RemoteControllers = 0;
				for (FConstPlayerControllerIterator It = Server.World->GetPlayerControllerIterator(); It; ++It)
				{
					const AStealthPlayerController* PC = Cast<AStealthPlayerController>(It->Get());
					ASSERT_THAT(IsNotNull(PC, TEXT("server controller is AStealthPlayerController")));
					ASSERT_THAT(IsTrue(PC->GetServerPossessionEpoch() >= 1, TEXT("OnPossess advanced the server epoch")));
					ASSERT_THAT(IsTrue(PC->GetReplySink().Index != 0, TEXT("each connection has a live reply sink")));
					RemoteControllers += PC->IsLocalController() ? 0 : 1;
				}
				ASSERT_THAT(AreEqual(1, RemoteControllers, TEXT("one remote client")));
			});
	}
};

#endif // ENABLE_PIE_NETWORK_TEST

#endif // WITH_DEV_AUTOMATION_TESTS
