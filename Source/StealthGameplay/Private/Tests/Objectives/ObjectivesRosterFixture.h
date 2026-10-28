#pragma once

// A real URosterStore for the StealthGame.Objectives.* rebind and reset tests (stories oc-011, oc-012): an initialised store on
// a transient game instance (fake clock, fault sink and entropy), a 2v2 session with teams locked and the round reset, so the
// service's subscriptions receive the store's own OnParticipantViewRebound and RosterReset events. Test-only.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Engine/GameInstance.h"
#include "Server/Random/StealthServerSeeds.h"
#include "Server/Roster/StealthRosterStore.h"
#include "Server/Testing/StealthFakeClock.h"
#include "Server/Testing/StealthFaultSinkTestSupport.h"
#include "UObject/Package.h"
#include "View/StealthRosterTeamView.h"

namespace StealthObjectivesTest
{
	/** Deterministic entropy that never runs out. */
	class FObjectivesFixtureEntropy final : public IEntropySource
	{
	public:
		virtual bool Fill(TArrayView<uint8> Out) override
		{
			for (int32 Index = 0; Index < Out.Num(); ++Index)
			{
				Out[Index] = static_cast<uint8>(0x41 + (Cursor++ % 24));
			}
			return true;
		}
		int32 Cursor = 0;
	};

	/** A 2v2 store: Ids[0], Ids[1] on team 0; Ids[2], Ids[3] on team 1; teams locked, round reset once. */
	struct FRosterStoreFixture
	{
		static constexpr int32 TeamSize = 2;
		static constexpr int32 Players = 4;

		FFakeStealthClock Clock{ 10.0 };
		FStealthRecordingFaultSink Sink;
		FObjectivesFixtureEntropy Entropy;
		UGameInstance* GameInstance = nullptr;
		URosterStore* Store = nullptr;
		TArray<FParticipantId> Ids;
		TArray<TStrongObjectPtr<URosterTeamViewComponent>> RosterViews;

		FRosterStoreFixture()
		{
			GameInstance = NewObject<UGameInstance>(GetTransientPackage());
			GameInstance->AddToRoot();
			Store = NewObject<URosterStore>(GameInstance);
			Store->AddToRoot();
			FRosterStoreInitOverrides Overrides;
			Overrides.bAuthority = true;
			Overrides.FaultSink = &Sink;
			Overrides.Entropy = &Entropy;
			Overrides.Clock = &Clock;
			Store->InitializeStore(Overrides);
			Store->OpenSession(TeamSize);
			TArray<FRosterTeamAssignment> Batch;
			for (int32 Index = 0; Index < Players; ++Index)
			{
				Ids.Add(Store->RegisterParticipant(FIdentityKey(0x0B1EC71000000000ull, 500ull + static_cast<uint64>(Index)), FString::Printf(TEXT("OC%d"), Index)).Id);
				Batch.Add({ Ids[Index], Index < TeamSize ? 0 : 1 });
			}
			Store->AssignTeams(Batch);
			Store->SetTeamsLocked(true);
			Store->ResetAll(1);
		}

		~FRosterStoreFixture()
		{
			Store->ShutdownStore();
			Store->RemoveFromRoot();
			GameInstance->RemoveFromRoot();
		}

		int32 TeamOf(int32 Index) const { return Index < TeamSize ? 0 : 1; }

		/** The Roster's RebindView stand-in for player Index: registers a (new) Roster view component, which raises the event. */
		void Rebind(int32 Index)
		{
			URosterTeamViewComponent* View = NewObject<URosterTeamViewComponent>(GetTransientPackage(), NAME_None, RF_Transient);
			RosterViews.Emplace(View);
			Store->RegisterTeamView(Ids[Index], View);
		}
	};
}

#endif
