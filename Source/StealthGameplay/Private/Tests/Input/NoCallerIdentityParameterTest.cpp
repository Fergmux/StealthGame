// Story iif-006 (production/epics/input-interaction-framework/story-006-no-caller-identity-parameter.md), GDD AC 2
// (R2, Logic, U NoCallerIdentityParameter): no entry point, hub event or payload struct has a type or name matching
// Controller, PlayerState, Team, IsAI or IsPlayer; and a CI grep of the component source for IsPlayerControlled,
// IsLocallyControlled, IsNpc, GetTeam, NM_ListenServer (extended with IsBotControlled and the Roster binding query)
// returns 0 hits.
//
// How each surface is checked (pinned in this story):
//   - entry points: reflection of every UFUNCTION parameter of UPawnCommandComponent (story iif-005's UFUNCTION choice);
//   - payload structs: reflection, recursive (FNetPawnCommand, FNetCommandArgs, FClientCommandSeq, FCommandReply,
//     FHoldStatus, FAttackPayload, FHideCommand, FTraversalHint, FCourtFixtureId);
//   - FCommandPayload (ADR-0002 section 2a) is a plain struct with no reflection: its field names are mirrored here and
//     a static_assert on its size makes any change to it revisit this list;
//   - hub events: native multicast delegates are invisible to reflection, so their exact signatures are static_asserts;
//   - the grep: tools/ci/check-no-caller-identity.sh through tools/ci/lint/gate.sh (comments are stripped, so a doc
//     comment naming a forbidden call is not a hit; generated headers are excluded).
// Seeded negatives: a synthetic `bool bIsAI` field, a real reflected function with an AController parameter
// (APawn::ReceivePossessed) and a fixture tree whose source calls IsPlayerControlled must each fail their checker.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "IdentityNameChecker.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Server/ActionHub.h"
#include "Server/Core/PawnCommandTypes.h"
#include "Server/Core/StealthPawnCommandComponent.h"
#include "View/NetPawnCommand.h"
#include <type_traits>

// Hub event signatures: exactly (AStealthPawn*, EActionType[, ECancelReason]); none names a controller or PlayerState.
static_assert(std::is_same_v<FOnPawnAction, TMulticastDelegate<void(AStealthPawn*, EActionType)>>, "OnActionStarted/Completed signature");
static_assert(std::is_same_v<FOnPawnActionCancelled, TMulticastDelegate<void(AStealthPawn*, EActionType, ECancelReason)>>, "OnActionCancelled signature");
static_assert(std::is_same_v<decltype(UActionHub::OnActionStarted), FOnPawnAction>, "OnActionStarted type");
static_assert(std::is_same_v<decltype(UActionHub::OnActionCompleted), FOnPawnAction>, "OnActionCompleted type");
static_assert(std::is_same_v<decltype(UActionHub::OnActionCancelled), FOnPawnActionCancelled>, "OnActionCancelled type");


namespace NoCallerIdentityParameterTestData
{
	/** FCommandPayload's fields (StealthQueuePayloads.h), with their types. */
	const TArray<StealthIdentityCheck::FNamedField>& CommandPayloadFields()
	{
		static const TArray<StealthIdentityCheck::FNamedField> Fields = {
			{ TEXT("FCommandPayload"), TEXT("Pawn"), TEXT("FPawnHandle") },
			{ TEXT("FCommandPayload"), TEXT("ControlEpoch"), TEXT("uint32") },
			{ TEXT("FCommandPayload"), TEXT("ClientSeq"), TEXT("FClientCommandSeq") },
			{ TEXT("FCommandPayload"), TEXT("Sink"), TEXT("FReplySinkId") },
			{ TEXT("FCommandPayload"), TEXT("Kind"), TEXT("uint8") },
			{ TEXT("FCommandPayload"), TEXT("Phase"), TEXT("uint8") },
			{ TEXT("FCommandPayload"), TEXT("Target"), TEXT("FSubjectRef") },
			{ TEXT("FCommandPayload"), TEXT("WorldPoint"), TEXT("FVector") },
			{ TEXT("FCommandPayload"), TEXT("Arg"), TEXT("uint32") },
		};
		return Fields;
	}

	FString FindBash()
	{
		const FString FromEnv = FPlatformMisc::GetEnvironmentVariable(TEXT("STEALTH_BASH"));
		if (!FromEnv.IsEmpty() && FPaths::FileExists(FromEnv))
		{
			return FromEnv;
		}
		for (const TCHAR* EnvVar : { TEXT("ProgramW6432"), TEXT("ProgramFiles") })
		{
			const FString Candidate = FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(EnvVar), TEXT("Git/bin/bash.exe"));
			if (FPaths::FileExists(Candidate))
			{
				return Candidate;
			}
		}
		return FString();
	}

	/** Runs the gate on the grep rule; Root empty = the repo. Returns (exit code, output). */
	TPair<int32, FString> RunGrep(const FString& Bash, const FString& Root)
	{
		const FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		const FString Gate = FPaths::Combine(Project, TEXT("tools/ci/lint/gate.sh"));
		const FString Rule = FPaths::Combine(Project, TEXT("tools/ci/check-no-caller-identity.sh"));
		const FString Args = Root.IsEmpty() ? FString::Printf(TEXT("\"%s\" \"%s\""), *Gate, *Rule)
			: FString::Printf(TEXT("\"%s\" --root \"%s\" \"%s\""), *Gate, *Root, *Rule);
		int32 Code = -1;
		FString Out;
		FString Err;
		FPlatformProcess::ExecProcess(*Bash, *Args, &Code, &Out, &Err);
		return { Code, Out + Err };
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputNoCallerIdentityParameterTest, "StealthGame.Input.NoCallerIdentityParameter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInputNoCallerIdentityParameterTest::RunTest(const FString& Parameters)
{
	using namespace NoCallerIdentityParameterTestData;
	using namespace StealthIdentityCheck;

	// Entry points: every parameter of every UFUNCTION on the component.
	int32 EntryPoints = 0;
	for (TFieldIterator<UFunction> It(UPawnCommandComponent::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		++EntryPoints;
		TestFalse(FString::Printf(TEXT("entry point name %s"), *It->GetName()), NameMatchesIdentity(It->GetName()));
		const TArray<FString> Violations = FindStructViolations(*It);
		TestTrue(FString::Printf(TEXT("%s parameters carry no identity (%s)"), *It->GetName(), *FString::Join(Violations, TEXT(", "))), Violations.IsEmpty());
	}
	TestEqual(TEXT("all nine entry points were scanned"), EntryPoints, 9);

	// Payload structs (reflected).
	const UScriptStruct* Structs[] = { FNetPawnCommand::StaticStruct(), FNetCommandArgs::StaticStruct(), FClientCommandSeq::StaticStruct(),
		FCommandReply::StaticStruct(), FHoldStatus::StaticStruct(), FAttackPayload::StaticStruct(), FHideCommand::StaticStruct(),
		FTraversalHint::StaticStruct(), FCourtFixtureId::StaticStruct() };
	for (const UScriptStruct* Struct : Structs)
	{
		const TArray<FString> Violations = FindStructViolations(Struct);
		TestTrue(FString::Printf(TEXT("%s carries no identity (%s)"), *Struct->GetName(), *FString::Join(Violations, TEXT(", "))), Violations.IsEmpty());
	}
	// FCommandPayload (plain, mirrored). Its size is the tripwire: a changed layout must revisit the mirrored list.
	TestEqual(TEXT("FCommandPayload size is unchanged (else update CommandPayloadFields)"), static_cast<int32>(sizeof(FCommandPayload)), 80);
	const TArray<FString> PayloadViolations = FindViolations(CommandPayloadFields());
	TestTrue(FString::Printf(TEXT("FCommandPayload carries no identity (%s)"), *FString::Join(PayloadViolations, TEXT(", "))), PayloadViolations.IsEmpty());

	// Seeded negatives: the checker is not blind.
	TestEqual(TEXT("a seeded bool bIsAI field fails the checker"), FindViolations({ { TEXT("FSeeded"), TEXT("bIsAI"), TEXT("bool") } }).Num(), 1);
	const UFunction* WithController = APawn::StaticClass()->FindFunctionByName(TEXT("ReceivePossessed"));
	if (TestNotNull(TEXT("APawn::ReceivePossessed is reflected (negative control)"), WithController))
	{
		TestTrue(TEXT("a reflected AController parameter fails the checker"), FindStructViolations(WithController).Num() > 0);
	}
	TestFalse(TEXT("ControlEpoch is not an identity name"), NameMatchesIdentity(TEXT("ControlEpoch")));

	// The CI grep over the component source, and its seeded fixture.
	const FString Bash = FindBash();
	if (!TestFalse(TEXT("a bash for the CI grep was found (set STEALTH_BASH to override)"), Bash.IsEmpty()))
	{
		return false;
	}
	const TPair<int32, FString> Real = RunGrep(Bash, FString());
	AddInfo(Real.Value);
	TestEqual(TEXT("the grep over the component source: gate exit 0 (zero hits)"), Real.Key, 0);
	TestTrue(TEXT("the grep scanned files and passed"), Real.Value.Contains(TEXT("GATE: PASS")));

	const FString Fixture = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/NoCallerIdentityFixture")));
	IFileManager::Get().DeleteDirectory(*Fixture, false, true);
	const FString Seeded = FPaths::Combine(Fixture, TEXT("Source/StealthGameplay/Private/Core/SeededIdentity.cpp"));
	FFileHelper::SaveStringToFile(TEXT("// IsPlayerControlled in a comment is not a hit\nvoid F(APawn* P)\n{\n\tif (P->IsPlayerControlled()) {}\n}\n"), *Seeded);
	const TPair<int32, FString> Negative = RunGrep(Bash, Fixture);
	AddInfo(Negative.Value);
	TestEqual(TEXT("the seeded fixture fails the grep (gate exit 1)"), Negative.Key, 1);
	TestTrue(TEXT("exactly the code line is reported, not the comment"), Negative.Value.Contains(TEXT("SeededIdentity.cpp:4 caller-identity")) && !Negative.Value.Contains(TEXT("SeededIdentity.cpp:1 ")));
	IFileManager::Get().DeleteDirectory(*Fixture, false, true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
