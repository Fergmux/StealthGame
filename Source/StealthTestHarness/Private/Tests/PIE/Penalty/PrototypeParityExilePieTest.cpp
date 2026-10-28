// NPC-Kill Penalty story nkp-021, AC1 (derived; GDD Prototype Disposition, first-playable networked test A "3rd civilian
// kill exiles the player; the round continues while a teammate is active", production/first-playable-report.md) ported to
// the production path: StealthGame.Penalty.PIE.PrototypeParityExile.ThirdUngracedNpcKillExilesTeammateStaysActive.
// Listen server + 2 clients on L_Prototype with a crowd, 2v1, L = 3 (the schema default). A pair player makes three
// ungraced NPC kills through the production strike path (commands on Round's queue, resolved in Assassination's slot,
// counted by UPenaltyService; never the prototype StealthBot driver): after kills 1 and 2 it is still Active (strike count
// 1, 2); the third exiles it (Roster Informant with reason StrikeLimit, strike count 3); its teammate stays Active and the
// round stays Active. Replaces the retired prototype rule test StealthGame.Rules.NpcKillRemoval with the
// boundary it checked (removal exactly on the limit-th kill); its "zero limit disables the penalty" case is not carried
// over: TDC clamps an authored 0 to the hard minimum 2.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Server/Core/Round/StealthRoundMatchStore.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Penalty/PenaltyPresentationPieSupport.h"

namespace PrototypeParityExilePieData
{
	constexpr double SettleSec = 0.6;
}

NETWORK_TEST_CLASS(PrototypeParityExile, "StealthGame.Penalty.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TWeakObjectPtr<AStealthPlayerController> Pair[2];
	FParticipantId Offender;
	FParticipantId Teammate;
	uint32 Seq = 0x56000000u;
	double Mark = 0.0;
	TArray<FString> StatesAfterKill;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(StealthAssassinationPie::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
		UStealthSettings* Settings = GetMutableDefault<UStealthSettings>();
		PreviousNpcCount = Settings->NpcCount;
		Settings->NpcCount = StealthPenaltyPie::CrowdNpcCount;
		StealthPenaltyHarnessSeams::SetCrowdlessTeamStartRespawn(false);
	}

	AFTER_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName); Timeout && !PreviousNetworkTimeout.IsEmpty())
		{
			Timeout->Set(*PreviousNetworkTimeout, ECVF_SetByCode);
		}
		if (PreviousNpcCount >= 0)
		{
			GetMutableDefault<UStealthSettings>()->NpcCount = PreviousNpcCount;
		}
	}

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(2).WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	double Now() const { return StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()); }

	void Wait(const TCHAR* Label, double Seconds)
	{
		Network
			.Then(*FString::Printf(TEXT("%s: mark"), Label), [this]() { Mark = Now(); })
			.Until(Label, [this, Seconds]() { return Now() >= Mark + Seconds; });
	}

	static TOptional<EParticipantState> StateOf(FParticipantId Participant)
	{
		const URosterStore* Store = StealthPenaltyPie::Roster();
		const FParticipantRecord* Record = Store != nullptr ? Store->FindRecord(Participant) : nullptr;
		return Record != nullptr ? TOptional<EParticipantState>(Record->State) : TOptional<EParticipantState>();
	}

	static bool RoundIsActive()
	{
		return StealthAssassinationPie::RoundActive();
	}

	TEST_METHOD(ThirdUngracedNpcKillExilesTeammateStaysActive)
	{
		using namespace StealthAssassinationPie;
		using namespace StealthPenaltyPresentationPie;
		const int32 L = Limit();
		Network
			.Until(TEXT("the round is Active, the crowd is up and the players split 2v1"), [this]()
			{
				AStealthPlayerController* S = nullptr;
				AStealthPlayerController* P[2] = { nullptr, nullptr };
				if (!RoundActive() || !SplitTeams(S, P) || StealthPenaltyPie::Penalty() == nullptr || StealthPenaltyPie::Roster() == nullptr)
				{
					return false;
				}
				Pair[0] = P[0];
				Pair[1] = P[1];
				Offender = StealthPenaltyPie::ParticipantOf(P[0]);
				Teammate = StealthPenaltyPie::ParticipantOf(P[1]);
				return Pair[0]->GetPawn() != nullptr;
			})
			.Then(TEXT("the offender stands clear of the bushes"), [this]() { MoveClearOfBushes(Pair[0]->GetPawn()); });
		Wait(TEXT("settle"), PrototypeParityExilePieData::SettleSec);
		for (int32 Kill = 0; Kill < L; ++Kill)
		{
			Network.Then(*FString::Printf(TEXT("ungraced NPC kill %d of %d"), Kill + 1, L), [this]()
			{
				NpcKill(Cast<AStealthPawn>(Pair[0]->GetPawn()), Seq);
			});
			Wait(TEXT("lockout"), StrikeStepSec);
			Network.Then(*FString::Printf(TEXT("record after kill %d"), Kill + 1), [this, Kill]()
			{
				const TOptional<EParticipantState> State = StateOf(Offender);
				const UPenaltyService* Penalty = StealthPenaltyPie::Penalty();
				StatesAfterKill.Add(FString::Printf(TEXT("kill %d: state %d strikes %d tally %d teammate %d round active %d"), Kill + 1,
					State.IsSet() ? static_cast<int32>(State.GetValue()) : -1, Penalty != nullptr ? Penalty->GetStrikeCountForTest(Offender) : -1,
					Penalty != nullptr ? Penalty->GetWrongTargetTallyForTest(Offender) : -1, StateOf(Teammate).IsSet() ? static_cast<int32>(StateOf(Teammate).GetValue()) : -1,
					RoundIsActive() ? 1 : 0));
			});
		}
		Network.Then(TEXT("assert prototype test A on the production path"), [this, L]()
		{
			for (const FString& Line : StatesAfterKill)
			{
				TestRunner->AddInfo(FString::Printf(TEXT("NKP021 A %s"), *Line));
			}
			ASSERT_THAT(IsTrue(StatesAfterKill.Num() == L, TEXT("fixture: every kill was recorded")));
			for (int32 Kill = 1; Kill < L; ++Kill)
			{
				ASSERT_THAT(IsTrue(StatesAfterKill[Kill - 1].Contains(FString::Printf(TEXT("state %d strikes %d "), static_cast<int32>(EParticipantState::Active), Kill)),
					*FString::Printf(TEXT("kill %d does not exile (Active, strike count %d)"), Kill, Kill)));
			}
			const TOptional<EParticipantState> State = StateOf(Offender);
			const URosterStore* Store = StealthPenaltyPie::Roster();
			const FParticipantRecord* Record = Store != nullptr ? Store->FindRecord(Offender) : nullptr;
			ASSERT_THAT(IsTrue(State.IsSet() && State.GetValue() == EParticipantState::Informant, TEXT("the third ungraced NPC kill exiles the player (Informant)")));
			ASSERT_THAT(IsTrue(Record != nullptr && Record->LastTransitionReason == ERosterReason::StrikeLimit, TEXT("the removal reason is StrikeLimit")));
			ASSERT_THAT(IsTrue(StealthPenaltyPie::Penalty()->GetStrikeCountForTest(Offender) == L, TEXT("the strike count reached L on the exiling kill")));
			ASSERT_THAT(IsTrue(StateOf(Teammate).IsSet() && StateOf(Teammate).GetValue() == EParticipantState::Active, TEXT("the teammate stays Active")));
			ASSERT_THAT(IsTrue(RoundIsActive(), TEXT("the round continues (still Active)")));
		});
	}
};

#endif
