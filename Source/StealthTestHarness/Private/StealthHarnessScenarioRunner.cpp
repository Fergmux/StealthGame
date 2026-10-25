// Story tdc-018: see StealthHarnessScenarioRunner.h.
#include "StealthHarnessScenarioRunner.h"

#include "StealthTestHarnessModule.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/NetDriver.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Server/Testing/StealthNegativeControl.h"

using namespace StealthHarness;

namespace
{
	const TCHAR* FaultName(EStealthFault Fault)
	{
		switch (Fault)
		{
		case EStealthFault::InvariantViolation: return TEXT("InvariantViolation");
		case EStealthFault::DeadRegistration: return TEXT("DeadRegistration");
		case EStealthFault::RejectedTuning: return TEXT("RejectedTuning");
		case EStealthFault::StaleEpoch: return TEXT("StaleEpoch");
		default: return TEXT("Unknown");
		}
	}
}

FString FHarnessResult::ToJson() const
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("scenario"), Scenario);
	Root->SetStringField(TEXT("build"), Build);
	Root->SetStringField(TEXT("replicationSystem"), ToString(ReplicationSystem));
	Root->SetStringField(TEXT("netProfile"), NetProfile);
	Root->SetBoolField(TEXT("passed"), bPassed);
	Root->SetBoolField(TEXT("negativeControlDetected"), bNegativeControlDetected);

	TArray<TSharedPtr<FJsonValue>> FaultValues;
	for (const FHarnessFault& Fault : Faults)
	{
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("category"), Fault.Category);
		Entry->SetStringField(TEXT("source"), Fault.Source.ToString());
		Entry->SetNumberField(TEXT("detail"), static_cast<double>(Fault.Detail));
		FaultValues.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Root->SetArrayField(TEXT("faults"), FaultValues);

	TArray<TSharedPtr<FJsonValue>> CaptureValues;
	for (const FString& Capture : Captures)
	{
		CaptureValues.Add(MakeShared<FJsonValueString>(Capture));
	}
	Root->SetArrayField(TEXT("captures"), CaptureValues);

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

EHarnessReplicationSystem FStealthNetDriverReplicationProbe::Read() const
{
	const UNetDriver* Driver = GetDriver ? GetDriver() : nullptr;
	if (Driver == nullptr)
	{
		return EHarnessReplicationSystem::Unknown;
	}
	return Driver->IsUsingIrisReplication() ? EHarnessReplicationSystem::Iris : EHarnessReplicationSystem::Legacy;
}

FStealthHarnessScenarioRunner::FStealthHarnessScenarioRunner(const IStealthReplicationSystemProbe& InProbe, FString InScenario, FString InBuild,
	FString InNetProfile, EHarnessReplicationSystem InExpected)
	: Probe(InProbe)
	, Expected(InExpected)
{
	Result.Scenario = MoveTemp(InScenario);
	Result.Build = MoveTemp(InBuild);
	Result.NetProfile = InNetProfile.IsEmpty() ? FString(TEXT("none")) : MoveTemp(InNetProfile);
}

bool FStealthHarnessScenarioRunner::AssertReplicationSystem()
{
	bAsserted = true;
	Result.ReplicationSystem = Probe.Read();
	const bool bMatches = Result.ReplicationSystem == Expected;
	if (!bMatches)
	{
		MarkFailed(FString::Printf(TEXT("replication system is %s, the pinned expectation is %s"), ToString(Result.ReplicationSystem), ToString(Expected)));
	}

	// The negative control: the same comparison against a probe reporting the OTHER system must be
	// detected as a failure. A comparison that cannot fail would make the assertion above vacuous.
	const EHarnessReplicationSystem Other = (Expected == EHarnessReplicationSystem::Legacy) ? EHarnessReplicationSystem::Iris : EHarnessReplicationSystem::Legacy;
	const FStealthFixedReplicationProbe OtherProbe(Other);
	const FStealthNegativeControl Control(TEXT("ReplicationSystemMismatch"));
	const FStealthNegativeControlResult ControlResult = Control.Run([&](FStealthCheckContext& Context)
	{
		Context.TestTrue(TEXT("probe reports the pinned replication system"), OtherProbe.Read() == Expected);
	});
	Result.bNegativeControlDetected = ControlResult.bDetected;
	if (!ControlResult.bDetected)
	{
		MarkFailed(TEXT("negative control not detected: the replication-system comparison cannot fail"));
	}

	UE_LOGF(LogStealthHarness, Display, "Replication system %ls (expected %ls), negative control %ls", ToString(Result.ReplicationSystem), ToString(Expected),
		Result.bNegativeControlDetected ? TEXT("detected") : TEXT("NOT detected"));
	return bMatches && ControlResult.bDetected;
}

void FStealthHarnessScenarioRunner::MarkFailed(const FString& Reason)
{
	bPassing = false;
	FailureReasons.Add(Reason);
	UE_LOGF(LogStealthHarness, Warning, "Harness run failed: %ls", *Reason);
}

void FStealthHarnessScenarioRunner::Report(EStealthFault Fault, FName Source, int64 Detail)
{
	Result.Faults.Add({ FaultName(Fault), Source, Detail });
	bPassing = false;
	FailureReasons.Add(FString::Printf(TEXT("fault %s from %s (%lld)"), FaultName(Fault), *Source.ToString(), Detail));
}

EStealthHarnessResultWrite FStealthHarnessScenarioRunner::WriteResult(const FString& Path)
{
	if (!bAsserted)
	{
		MarkFailed(TEXT("the replication system was never asserted"));
	}
	Result.bPassed = bPassing;
	if (Path.IsEmpty())
	{
		UE_LOGF(LogStealthHarness, Display, "No -StealthHarnessOut: result not written (scenario %ls, replicationSystem %ls, passed %d)",
			*Result.Scenario, ToString(Result.ReplicationSystem), (int32)Result.bPassed);
		return EStealthHarnessResultWrite::NotRequested;
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!FFileHelper::SaveStringToFile(Result.ToJson(), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		MarkFailed(FString::Printf(TEXT("the result path is not writable: %s"), *Path));
		Result.bPassed = false;
		return EStealthHarnessResultWrite::Failed;
	}
	UE_LOGF(LogStealthHarness, Display, "Result written to %ls (passed %d, replicationSystem %ls)", *Path, (int32)Result.bPassed, ToString(Result.ReplicationSystem));
	return EStealthHarnessResultWrite::Written;
}

FString FStealthHarnessScenarioRunner::ResolveResultPath(const FString& OutParam, bool bServer, int32 ClientIndex)
{
	if (OutParam.IsEmpty())
	{
		return FString();
	}
	if (OutParam.EndsWith(TEXT(".json"), ESearchCase::IgnoreCase))
	{
		return OutParam;
	}
	const FString FileName = bServer ? FString(TEXT("server-result.json")) : FString::Printf(TEXT("client%d-result.json"), ClientIndex);
	return FPaths::Combine(OutParam, FileName);
}
