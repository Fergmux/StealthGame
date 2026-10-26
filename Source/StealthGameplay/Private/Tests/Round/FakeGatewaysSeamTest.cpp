// Story rmf-003 (production/epics/round-match-framework/story-003-test-seam-fake-gateways.md):
// GDD test seam 3. Fake Session, Roster, TDC, Character Pool, Level Socket and Crowd gateways are supplied by
// dependency injection (no singleton access) and record their calls in order, on one shared call-order log.
// Strict fakes: a call with no scripted answer is caught (negative control below).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Testing/StealthNegativeControl.h"
#include "Server/Testing/StealthRoundFakeGateways.h"

namespace FakeGatewaysSeamTestData
{
	constexpr uint32 RoundIndex = 1;
	constexpr int32 NpcCount = 50;
	constexpr int32 FixtureSeed = 1234;
	constexpr uint64 FixtureTuningHash = 0x00000000000A11CEull;

	/**
	 * A stand-in system under test: it receives every collaborator through FRoundGateways and nothing else, then
	 * makes the R5 acceptance calls and the R6 P2-P3 / P6 calls in their GDD order, interleaved across gateways.
	 */
	class FStubStartSequence
	{
	public:
		explicit FStubStartSequence(const FRoundGateways& InGateways) : Gateways(InGateways) {}

		bool Run()
		{
			if (!Gateways.IsComplete() || !Gateways.Session->CanRequestRoundTravel())
			{
				return false;
			}
			Gateways.Session->SetAdmissionMode(ERoundAdmissionMode::RejoinOnly);
			Gateways.Roster->SetTeamsLocked(true);
			Gateways.Roster->ResetAll(RoundIndex);
			if (Gateways.Session->RequestRoundTravel(FName(TEXT("Map_Graybox")), FName(TEXT("Playtest")), RoundIndex) != ERoundTravelRequestResult::Accepted)
			{
				return false;
			}
			Gateways.Tuning->SignalPreparingEntered();
			const FParticipantId Players[] = { FParticipantId(0), FParticipantId(1) };
			Gateways.LevelSocket->RequestLayout(FixtureSeed, FixtureTuningHash);
			Gateways.CharacterPool->AllocateAppearances(Players, NpcCount);
			Gateways.Session->GetServerSubSeed(ESeedDomain::SpawnOrder, 0);
			Gateways.LevelSocket->RequestStartPlacement(TConstArrayView<FParticipantId>(Players, 1), TConstArrayView<FParticipantId>(Players + 1, 1), NpcCount);
			Gateways.Roster->RecordAppearance(Players[0], Gateways.CharacterPool->GetAssignedAppearance(Players[0]));
			Gateways.Crowd->Halt();
			return true;
		}

	private:
		FRoundGateways Gateways;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundFakeGatewaysSeamTest, "StealthGame.Round.FakeGatewaysSeam",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundFakeGatewaysSeamTest::RunTest(const FString& Parameters)
{
	using namespace FakeGatewaysSeamTestData;

	// Arrange: six fakes on one shared, test-owned log; every answer scripted.
	FRoundFakeCallLog Log;
	FRoundFakeGatewaySet Fakes(Log);
	Fakes.Session.Script(TEXT("CanRequestRoundTravel"), 1);
	Fakes.Session.Script(TEXT("SetAdmissionMode"));
	Fakes.Roster.Script(TEXT("SetTeamsLocked"), static_cast<int64>(ERosterResult::Applied));
	Fakes.Roster.Script(TEXT("ResetAll"), static_cast<int64>(ERosterResult::Applied));
	Fakes.Session.Script(TEXT("RequestRoundTravel"), static_cast<int64>(ERoundTravelRequestResult::Accepted));
	Fakes.Tuning.Script(TEXT("SignalPreparingEntered"));
	Fakes.LevelSocket.Script(TEXT("RequestLayout"));
	Fakes.CharacterPool.Script(TEXT("AllocateAppearances"), static_cast<int64>(ERoundAllocateResult::Ok));
	Fakes.Session.Script(TEXT("GetServerSubSeed"), 1);
	Fakes.LevelSocket.Script(TEXT("RequestStartPlacement"), static_cast<int64>(ERoundPlacementResult::Ok));
	Fakes.CharacterPool.Script(TEXT("GetAssignedAppearance"), 7);
	Fakes.Roster.Script(TEXT("RecordAppearance"), static_cast<int64>(ERosterResult::Applied));
	Fakes.Crowd.Script(TEXT("Halt"));

	// Act: the system under test is built from the injected bundle only (no GetSubsystem, global or static).
	const FRoundGateways Gateways = Fakes.AsGateways();
	TestTrue(TEXT("all six gateways injected"), Gateways.IsComplete());
	FStubStartSequence Sut(Gateways);
	const bool bRan = Sut.Run();

	// Assert: one shared order across gateways, each fake's own list, nothing unexpected.
	TestTrue(TEXT("the scenario ran to completion"), bRan);
	using G = ERoundFakeGateway;
	const TArray<TPair<G, FName>> Expected = {
		{ G::Session, TEXT("CanRequestRoundTravel") }, { G::Session, TEXT("SetAdmissionMode") }, { G::Roster, TEXT("SetTeamsLocked") },
		{ G::Roster, TEXT("ResetAll") }, { G::Session, TEXT("RequestRoundTravel") }, { G::Tuning, TEXT("SignalPreparingEntered") },
		{ G::LevelSocket, TEXT("RequestLayout") }, { G::CharacterPool, TEXT("AllocateAppearances") }, { G::Session, TEXT("GetServerSubSeed") },
		{ G::LevelSocket, TEXT("RequestStartPlacement") }, { G::CharacterPool, TEXT("GetAssignedAppearance") }, { G::Roster, TEXT("RecordAppearance") },
		{ G::Crowd, TEXT("Halt") },
	};
	if (TestEqual(TEXT("shared log holds every call"), Log.Calls.Num(), Expected.Num()))
	{
		for (int32 Index = 0; Index < Expected.Num(); ++Index)
		{
			TestTrue(FString::Printf(TEXT("call %d is %s on gateway %d"), Index, *Expected[Index].Value.ToString(), static_cast<int32>(Expected[Index].Key)),
				Log.Calls[Index].Gateway == Expected[Index].Key && Log.Calls[Index].Call == Expected[Index].Value);
		}
	}
	TestTrue(TEXT("no unexpected (unscripted) call"), Log.Unexpected.IsEmpty());

	// Each fake's own list equals the shared log filtered to that gateway, in order.
	const auto CheckOwn = [this, &Log](const FRoundFakeGatewayBase& Fake, G Gateway, const TCHAR* Name)
	{
		const TArray<FRoundFakeCall> Filtered = Log.Calls.FilterByPredicate([Gateway](const FRoundFakeCall& C) { return C.Gateway == Gateway; });
		TestTrue(FString::Printf(TEXT("%s fake's own list matches the shared order"), Name), Fake.OwnCalls == Filtered);
	};
	CheckOwn(Fakes.Session, G::Session, TEXT("Session"));
	CheckOwn(Fakes.Roster, G::Roster, TEXT("Roster"));
	CheckOwn(Fakes.Tuning, G::Tuning, TEXT("Tuning"));
	CheckOwn(Fakes.CharacterPool, G::CharacterPool, TEXT("CharacterPool"));
	CheckOwn(Fakes.LevelSocket, G::LevelSocket, TEXT("LevelSocket"));
	CheckOwn(Fakes.Crowd, G::Crowd, TEXT("Crowd"));

	// Arguments are summarised as ids, enums and counts.
	if (Log.Calls.Num() == Expected.Num())
	{
		TestTrue(TEXT("ResetAll carries the round index"), Log.Calls[3].Args == TArray<int64>{ RoundIndex });
		TestTrue(TEXT("SetAdmissionMode carries RejoinOnly"), Log.Calls[1].Args == TArray<int64>{ static_cast<int64>(ERoundAdmissionMode::RejoinOnly) });
		TestTrue(TEXT("AllocateAppearances carries counts only"), Log.Calls[7].Args == TArray<int64>{ 2, NpcCount });
		TestTrue(TEXT("RecordAppearance carries the scripted card"), Log.Calls[11].Args == TArray<int64>{ 0, 7 });
	}

	// Negative control: an unscripted call is caught (strict fakes), so a silent unexpected call cannot pass.
	FStealthNegativeControl Control(TEXT("UnscriptedGatewayCall"));
	const FStealthNegativeControlResult Result = Control.Run([](FStealthCheckContext& Ctx)
	{
		FRoundFakeCallLog StrictLog;
		FRoundFakeGatewaySet StrictFakes(StrictLog);
		StrictFakes.Roster.SetTeamSize(4);  // nothing scripted
		Ctx.TestTrue(TEXT("no unexpected call"), StrictLog.Unexpected.IsEmpty());
	});
	FStealthNegativeControl::Report(*this, Result);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
