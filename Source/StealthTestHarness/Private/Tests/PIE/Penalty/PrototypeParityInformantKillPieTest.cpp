// NPC-Kill Penalty story nkp-021, AC2 (derived; GDD Prototype Disposition, first-playable networked test B "killing an
// enemy informant respawns them as an informant; not counted as an NPC kill", production/first-playable-report.md) ported
// to the production path: StealthGame.Penalty.PIE.PrototypeParityInformantKill.InformantKillRebodiesByTakeoverNoStrike.
// Listen server + 2 clients on L_Prototype with a crowd, 2v1. The solo player S kills a pair player P (Assassinated); P's
// informant takes over a crowd pawn; S kills that informant body. Then:
//   - S's tally is + 1 (WrongTarget) and its strike count is unchanged (0: an informant kill is no NPC kill, PEN-R2), and S
//     is still Active (not exiled);
//   - after the preset's InformantRespawnDelaySec P is re-bodied by the takeover of a crowd pawn that existed before its
//     T_due (no pawn spawned for it: the prototype spawned one; same intent, different mechanism), still an informant.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Penalty/PenaltyPresentationPieSupport.h"

namespace PrototypeParityInformantKillPieData
{
	constexpr double SettleSec = 0.6;
	constexpr double BodySettleSec = 1.0;
}

NETWORK_TEST_CLASS(PrototypeParityInformantKill, "StealthGame.Penalty.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TWeakObjectPtr<AStealthPlayerController> Solo;
	TWeakObjectPtr<AStealthPlayerController> Victim;
	FParticipantId AttackerId;
	FParticipantId VictimId;
	TArray<TWeakObjectPtr<APawn>> OldBodies;
	TWeakObjectPtr<APawn> InformantBody;
	StealthPenaltyPie::FSpawnTracker Spawns;
	uint32 Seq = 0x57000000u;
	double Mark = 0.0;
	uint8 TallyBefore = 0;
	uint8 StrikesBefore = 0;
	double KillAt = 0.0;
	double Due = 0.0;

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

	AFTER_EACH()
	{
		Spawns.Stop();
	}

	double Now() const { return StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()); }

	void Wait(const TCHAR* Label, double Seconds)
	{
		Network
			.Then(*FString::Printf(TEXT("%s: mark"), Label), [this]() { Mark = Now(); })
			.Until(Label, [this, Seconds]() { return Now() >= Mark + Seconds; });
	}

	AStealthPawn* SoloPawn() const { return Solo.IsValid() ? Cast<AStealthPawn>(Solo->GetPawn()) : nullptr; }

	/** Waits until the victim is Embodied in a body it never had (records it). */
	void AwaitNewBody(const TCHAR* Label)
	{
		Network.Until(Label, [this]()
		{
			const UPenaltyService* Penalty = StealthPenaltyPie::Penalty();
			APawn* Body = StealthPenaltyPie::BodyOf(VictimId);
			if (Penalty == nullptr || Penalty->GetLifeState(VictimId) != EInformantLifeState::Embodied || Body == nullptr || OldBodies.Contains(Body))
			{
				if (Penalty != nullptr && Penalty->GetDueServerTime(VictimId).IsSet())
				{
					Due = Penalty->GetDueServerTime(VictimId).GetValue();
				}
				return false;
			}
			InformantBody = Body;
			return true;
		});
	}

	TEST_METHOD(InformantKillRebodiesByTakeoverNoStrike)
	{
		using namespace StealthAssassinationPie;
		Network
			.Until(TEXT("the round is Active, the crowd is up and the players split 2v1"), [this]()
			{
				AStealthPlayerController* S = nullptr;
				AStealthPlayerController* P[2] = { nullptr, nullptr };
				if (!RoundActive() || !SplitTeams(S, P) || StealthPenaltyPie::Penalty() == nullptr || StealthPenaltyPie::Roster() == nullptr)
				{
					return false;
				}
				Solo = S;
				Victim = P[0];
				AttackerId = StealthPenaltyPie::ParticipantOf(S);
				VictimId = StealthPenaltyPie::ParticipantOf(P[0]);
				return SoloPawn() != nullptr && Victim->GetPawn() != nullptr;
			})
			.Then(TEXT("S stands clear of the bushes"), [this]() { MoveClearOfBushes(SoloPawn()); });
		Wait(TEXT("settle"), PrototypeParityInformantKillPieData::SettleSec);
		Network.Then(TEXT("S kills P (Assassinated)"), [this]()
		{
			APawn* Target = Victim->GetPawn();
			OldBodies.Add(Target);
			PlaceFacing(Target, SoloPawn(), 150.0);
			StrikeAt(SoloPawn(), Target, Seq);
		});
		AwaitNewBody(TEXT("P's informant is embodied"));
		Wait(TEXT("the body settles"), PrototypeParityInformantKillPieData::BodySettleSec);
		Network
			.Then(TEXT("S kills P's informant body"), [this]()
			{
				const UPenaltyService* Penalty = StealthPenaltyPie::Penalty();
				TallyBefore = Penalty->GetWrongTargetTallyForTest(AttackerId);
				StrikesBefore = Penalty->GetStrikeCountForTest(AttackerId);
				OldBodies.Add(InformantBody);
				Spawns.Start(StealthAssassinationPie::ServerWorld());
				KillAt = Now();
				MoveClearOfBushes(InformantBody.Get());
				PlaceFacing(InformantBody.Get(), SoloPawn(), 150.0);
				StrikeAt(SoloPawn(), InformantBody.Get(), Seq);
			});
		AwaitNewBody(TEXT("P is re-bodied after the delay"));
		Network.Then(TEXT("assert prototype test B on the production path"), [this]()
		{
			const UPenaltyService* Penalty = StealthPenaltyPie::Penalty();
			const URosterStore* Store = StealthPenaltyPie::Roster();
			const FParticipantRecord* Attacker = Store->FindRecord(AttackerId);
			const FParticipantRecord* Rebodied = Store->FindRecord(VictimId);
			const double Delay = FPenaltyParams::FromTuning(FStealthServerTuning()).InformantRespawnDelaySec;
			TestRunner->AddInfo(FString::Printf(TEXT("NKP021 B: S tally %u -> %u, strikes %u -> %u; kill %.3f, T_due %.3f (D %.1f), body %s (existed before T_due %d, bound spawns %d)"),
				TallyBefore, Penalty->GetWrongTargetTallyForTest(AttackerId), StrikesBefore, Penalty->GetStrikeCountForTest(AttackerId), KillAt, Due, Delay,
				*GetNameSafe(InformantBody.Get()), Spawns.ExistedBefore(InformantBody.Get(), Due) ? 1 : 0, Spawns.CountBoundSpawnedSince(KillAt)));
			ASSERT_THAT(IsTrue(Penalty->GetWrongTargetTallyForTest(AttackerId) == TallyBefore + 1, TEXT("the attacker's tally is + 1 (WrongTarget)")));
			ASSERT_THAT(IsTrue(Penalty->GetStrikeCountForTest(AttackerId) == StrikesBefore, TEXT("the attacker's strike count is unchanged (an informant kill is no NPC kill)")));
			ASSERT_THAT(IsTrue(Attacker != nullptr && Attacker->State == EParticipantState::Active, TEXT("the attacker is not exiled")));
			ASSERT_THAT(IsTrue(Rebodied != nullptr && Rebodied->State == EParticipantState::Informant, TEXT("the victim is still an informant")));
			ASSERT_THAT(IsTrue(Due >= KillAt + Delay - 0.05, TEXT("the re-body waited the preset's InformantRespawnDelaySec (T_due >= kill + D)")));
			ASSERT_THAT(IsTrue(Spawns.CountBoundSpawnedSince(KillAt) == 0 && Spawns.ExistedBefore(InformantBody.Get(), Due),
				TEXT("re-bodied by the takeover of a crowd pawn that existed before T_due (no pawn spawned for it)")));
		});
	}
};

#endif
