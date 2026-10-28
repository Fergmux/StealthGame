// NPC-Kill Penalty story nkp-017, AC2 / AC-NKP-44 (PEN-R16; Penalty CR-18; HUD-R9 "Informant killed"; ADR-0008 T2):
// StealthGame.Penalty.PIE.InformantKilledView.NoCardViewOnFallingBodyCountdownAfterFade. Listen server + 2 clients on
// L_Prototype with a crowd, 2v1. The solo player kills the pair player on a client machine (Assassinated); after
// InformantRespawnDelaySec its informant takes over a crowd pawn (Embodied); the solo player then kills that informant
// body. On the informant's own machine, sampled every frame from the kill until the countdown shows:
//   - the view stays on its own falling body (the camera's view target is the killed informant body) until the fade
//     start DeathServerTime + DeathFallSec, and no frame before it is black;
//   - no removal card and no exile card are presented (no RemovalCard / ExileCard record, no card-entrance event): a killed
//     informant has no State change (PEN-R16, CR-18);
//   - the returning countdown appears only once the fade has completed: never before DeathServerTime + DeathFallSec +
//     NoPawnFadeSec (UI-NKP-4, HUD-E11), and it then counts down from the carried T_due.
// PENDING on the Camera epic (camera-system story 018 is not built; recorded on the story): the fade itself over
// NoPawnFadeSec starting at DeathServerTime + DeathFallSec (+- 1 frame). Measured times are logged ("NKP017 killed").

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Server/StealthTuningInvariants.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Penalty/PenaltyPresentationPieSupport.h"
#include "View/StealthTuningPublicSubstructs.h"

namespace InformantKilledViewPieData
{
	constexpr double SettleSec = 0.6;
	/** A respawned body settles this long before it is struck (every client has it). */
	constexpr double BodySettleSec = 1.0;
}

NETWORK_TEST_CLASS(InformantKilledView, "StealthGame.Penalty.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TWeakObjectPtr<AStealthPlayerController> Solo;
	TWeakObjectPtr<AStealthPlayerController> Victim;
	TWeakObjectPtr<AStealthPlayerController> Machine;
	FParticipantId VictimId;
	TWeakObjectPtr<APawn> FirstBody;
	TWeakObjectPtr<APawn> InformantBody;
	TWeakObjectPtr<APawn> ClientBody;
	uint32 Seq = 0x52000000u;
	double Mark = 0.0;
	int32 LogFrom = 0;
	StealthPenaltyPresentationPie::FMachineFrame Frames;
	int32 Samples = 0;
	int32 OffBodyBeforeFade = 0;
	int32 BlackBeforeFade = 0;
	double DeathServerTime = -1.0;
	double ReleasedAt = -1.0;
	int32 CardEvents = 0;
	FDelegateHandle CardHandle;

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
		if (Machine.IsValid() && CardHandle.IsValid())
		{
			Machine->OnRemovalCardEntered.Remove(CardHandle);
		}
		CardHandle.Reset();
	}

	double Now() const { return StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()); }

	void Wait(const TCHAR* Label, double Seconds)
	{
		Network
			.Then(*FString::Printf(TEXT("%s: mark"), Label), [this]() { Mark = Now(); })
			.Until(Label, [this, Seconds]() { return Now() >= Mark + Seconds; });
	}

	AStealthPawn* SoloPawn() const { return Solo.IsValid() ? Cast<AStealthPawn>(Solo->GetPawn()) : nullptr; }

	/** The fade start (DeathServerTime + DeathFallSec) and its end (+ NoPawnFadeSec). */
	double FadeStart() const { return DeathServerTime + static_cast<double>(FAssassinationTuning_Public().DeathFallSec); }
	double FadeEnd() const { return FadeStart() + StealthTuningPendingOwnerKeys::NoPawnFadeSec; }

	TEST_METHOD(NoCardViewOnFallingBodyCountdownAfterFade)
	{
		using namespace StealthAssassinationPie;
		using namespace StealthPenaltyPresentationPie;
		Network
			.Until(TEXT("the round is Active, the crowd is up, 2v1 and a pair player is a client"), [this]()
			{
				AStealthPlayerController* S = nullptr;
				AStealthPlayerController* P[2] = { nullptr, nullptr };
				if (!RoundActive() || !SplitTeams(S, P) || StealthPenaltyPie::Penalty() == nullptr)
				{
					return false;
				}
				AStealthPlayerController* Client = ClientPairPlayer(P);
				if (Client == nullptr || OwnerMachineController(Client) == nullptr)
				{
					return false;
				}
				Solo = S;
				Victim = Client;
				Machine = OwnerMachineController(Client);
				VictimId = StealthPenaltyPie::ParticipantOf(Client);
				return SoloPawn() != nullptr && Victim->GetPawn() != nullptr;
			})
			.Then(TEXT("the solo player stands clear of the bushes"), [this]() { MoveClearOfBushes(SoloPawn()); });
		Wait(TEXT("settle"), InformantKilledViewPieData::SettleSec);
		Network
			.Then(TEXT("the solo player kills the pair player (Assassinated)"), [this]()
			{
				APawn* Target = Victim->GetPawn();
				FirstBody = Target;
				PlaceFacing(Target, SoloPawn(), 150.0);
				StrikeAt(SoloPawn(), Target, Seq);
			})
			.Until(TEXT("the informant is embodied in a crowd pawn"), [this]()
			{
				const UPenaltyService* Penalty = StealthPenaltyPie::Penalty();
				APawn* Body = StealthPenaltyPie::BodyOf(VictimId);
				if (Penalty == nullptr || Penalty->GetLifeState(VictimId) != EInformantLifeState::Embodied || Body == nullptr || Body == FirstBody.Get())
				{
					return false;
				}
				InformantBody = Body;
				return true;
			});
		Wait(TEXT("the body settles on every machine"), InformantKilledViewPieData::BodySettleSec);
		Network
			.Until(TEXT("the informant's machine possesses its new body"), [this]()
			{
				return Machine.IsValid() && Machine->GetPawn() != nullptr && Machine->GetPawn() != FirstBody.Get();
			})
			.Then(TEXT("the solo player kills the informant body"), [this]()
			{
				ClientBody = Machine->GetPawn();
				LogFrom = Machine->GetOwnPresentationLog().Num();
				CardHandle = Machine->OnRemovalCardEntered.AddLambda([this](double) { ++CardEvents; });
				MoveClearOfBushes(InformantBody.Get());
				PlaceFacing(InformantBody.Get(), SoloPawn(), 150.0);
				StrikeAt(SoloPawn(), InformantBody.Get(), Seq);
			})
			.Until(TEXT("sample every frame until the countdown shows"), [this]()
			{
				const AStealthPlayerController* Local = Machine.Get();
				if (Local == nullptr)
				{
					return false;
				}
				Frames.Sample(Local);
				++Samples;
				const AStealthPawn* Copy = Cast<AStealthPawn>(ClientBody.Get());
				if (DeathServerTime < 0.0 && Copy != nullptr && Copy->HasDeathWrite())
				{
					DeathServerTime = Copy->GetDeathState().DeathServerTime;
				}
				const double LocalNow = MachineNow(Local);
				const FViewSample View = SampleView(Local);
				if (ReleasedAt < 0.0 && DeathServerTime >= 0.0 && View.Possessed != ClientBody.Get())
				{
					ReleasedAt = LocalNow;
				}
				if (DeathServerTime < 0.0 || LocalNow + 0.001 < FadeStart())
				{
					OffBodyBeforeFade += View.ViewTarget != ClientBody.Get() ? 1 : 0;
					BlackBeforeFade += IsBlack(View) ? 1 : 0;
				}
				return !RecordsOf(Local, EOwnPresentationEvent::Countdown, LogFrom).FilterByPredicate([](const FOwnPresentationRecord& R) { return R.Value >= 0; }).IsEmpty();
			})
			.Then(TEXT("assert the informant-killed view"), [this]()
			{
				const AStealthPlayerController* Local = Machine.Get();
				const TArray<FOwnPresentationRecord> Countdowns = RecordsOf(Local, EOwnPresentationEvent::Countdown, LogFrom).FilterByPredicate(
					[](const FOwnPresentationRecord& R) { return R.Value >= 0; });
				const TArray<FOwnPresentationRecord> Removal = RecordsOf(Local, EOwnPresentationEvent::RemovalCard, LogFrom);
				const TArray<FOwnPresentationRecord> Exile = RecordsOf(Local, EOwnPresentationEvent::ExileCard, LogFrom);
				const double FirstCountdown = Countdowns.Num() > 0 ? Countdowns[0].ServerNow : -1.0;
				TestRunner->AddInfo(FString::Printf(TEXT("NKP017 killed: death %.3f, fade start %.3f, fade end %.3f, released %.3f, first countdown '%s' at %.3f (gate %.3f); %d frames, %d off the body and %d black before the fade start; removal cards %d, exile cards %d, card events %d"),
					DeathServerTime, FadeStart(), FadeEnd(), ReleasedAt, Countdowns.Num() > 0 ? *Countdowns[0].Text : TEXT(""), FirstCountdown, Local != nullptr ? Local->GetCountdownGateAt() : -1.0,
					Samples, OffBodyBeforeFade, BlackBeforeFade, Removal.Num(), Exile.Num(), CardEvents));
				TestRunner->AddInfo(TEXT("NKP017 PENDING (camera-system story 018 not built): the NoPawnFadeSec fade starting at DeathServerTime + DeathFallSec"));
				ASSERT_THAT(IsTrue(DeathServerTime >= 0.0, TEXT("fixture: the informant body received its death write")));
				ASSERT_THAT(IsTrue(OffBodyBeforeFade == 0, TEXT("the view stays on the falling body until the fade start")));
				ASSERT_THAT(IsTrue(BlackBeforeFade == 0, TEXT("no frame is black before the fade start")));
				ASSERT_THAT(IsTrue(Removal.IsEmpty() && Exile.IsEmpty() && CardEvents == 0, TEXT("no removal card, no exile card, no card-entrance event for a killed informant")));
				ASSERT_THAT(IsTrue(FirstCountdown + 0.001 >= FadeEnd(), TEXT("the countdown appears only after the fade completes (DeathServerTime + DeathFallSec + NoPawnFadeSec)")));
				ASSERT_THAT(IsTrue(Countdowns[0].Text.StartsWith(TEXT("Returning in ")) || Countdowns[0].Text == TEXT("Returning"), TEXT("the countdown reads 'Returning in {s}' (or the static 'Returning')")));
			});
	}
};

#endif
