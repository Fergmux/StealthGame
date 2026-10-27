// Assassination story asn-022 AC2 / AC-ASN-25 (ASN-R21, VA-ASN-2; ADR-0018 section 5.1 and Verification Required (2)
// "ReopenFastForward", ADR-0003 S10-R5; ADR-0008 T2; listen server + 2 clients on L_Prototype, NpcCount 0, every PIE world at a
// fixed 60 Hz): StealthGame.Assassination.PIE.LateActorSeeksNeverSnaps.<method>.
// A bystander client B first receives a struck NPC's actor late: the victim is denied to B (StealthRelevancyDenySeam,
// Release Manual) and released by hand at DeathServerTime + 0.6 s (run 1) and + 7.5 s (run 2, past DeathFallSec 1.2 +
// BodyLingerSec 6.0):
//   run 1  B's instance carries FDeathState from its first sight; B's first clip is Death_Fall, late, sought to
//          EstimatedServerNow() - DeathServerTime (+- 1 frame; about 0.6 s plus the in-process delivery) and cross-faded
//          within 0.1 s; NO POSE POP (QA rewrite, bound fixed with S10's facts): over B's first 10 shown frames the largest
//          per-frame root-relative bone-position delta is <= the clip's own largest per-frame delta (the server's copy, which
//          played the fall from its start) + the blend allowance (the largest per-bone distance between the server's standing
//          pose before the death and its fall pose over B's blend window, times one frame / the blend time) + 3 cm. A
//          re-created pawn's collapsed first frame (S10 Q3b, ~1 m at the head) fails this; the fresh-actor warm-up
//          (S10-R5) prevents it;
//   run 2  B's instance carries FDeathState and is never shown on any sampled frame (hidden in its first frame, VA-ASN-2
//          NeverShow), with no clip commanded;
//   host   on the listen-server host and every other machine that received the victim normally, Death_Fall starts at
//          DeathServerTime (+- 1 frame), and DeathServerTime is the strike's ContactServerTime: the host's fall runs on the
//          same ContactServerTime schedule as the clients'.
// Harness expectations depend on spike S10 (production/qa/spikes/2026-10-25_S10.md, 2026-10-26_S10-rerun.md): a re-open is
// a new actor under the same NetGUID; the first frame of a re-created pawn is collapsed unless warmed up hidden.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Server/Core/StealthPawnFactory.h"
#include "Server/EncounterService.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/ReopenPieSupport.h"

namespace LateActorSeeksNeverSnapsPieData
{
	enum ERun : int32 { MidFall, PastLinger, RunCount };
	const TCHAR* Name(int32 Run) { return Run == MidFall ? TEXT("+0.6 s") : TEXT("+7.5 s"); }
	/** When B is released, after DeathServerTime (server seconds). */
	constexpr double ReleaseAfterSec[RunCount] = { 0.6, 7.5 };
	/** How long each run is watched after the release. */
	constexpr double WatchAfterReleaseSec[RunCount] = { 1.6, 1.0 };
	/** Delivery allowance of an in-process PIE connection (fixed frame rate, no emulated lag). */
	constexpr double DeliverySec = 0.15;
	/**
	 * The client's server-time estimate against the server's own clock (the engine's GetServerWorldTimeSeconds, the
	 * stand-in for ADR-0002 section 9's estimator, lags by about the one-way delay; story asn-017 uses the same bound).
	 */
	constexpr double ClockToleranceSec = 0.05;
	/** Frames of the pop check. */
	constexpr int32 PopFrames = 10;
	/** Per-machine noise of the sample AnimBP's foot placement per frame (story asn-018's parity finding), cm. */
	constexpr double PopToleranceCm = 3.0;
	constexpr double StrikeDistanceCm = 140.0;

	struct FRun
	{
		TWeakObjectPtr<AStealthPawn> Victim;
		const AStealthPawn* VictimKey = nullptr;
		/** The victim's copy on every machine (the bystander's slot unused), as each machine's log key. */
		TArray<const AStealthPawn*> Keys;
		StealthReopenPie::FInstanceWatch Client;
		StealthReopenPie::FServerBoneWatch ServerBones;
		double ContactServerTime = -1.0;
		double DeathServerTime = -1.0;
		double ReleaseServerTime = -1.0;
		bool bReleased = false;
	};
}

NETWORK_TEST_CLASS(LateActorSeeksNeverSnaps, "StealthGame.Assassination.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;
	inline static bool bPreviousFixedFrameRate = false;
	inline static float PreviousFixedFrameRate = 30.f;

	TWeakObjectPtr<AStealthPlayerController> Striker;
	TWeakObjectPtr<AStealthPlayerController> Bystander;
	int32 BystanderMachine = INDEX_NONE;
	int32 ThirdMachine = INDEX_NONE;
	LateActorSeeksNeverSnapsPieData::FRun Runs[LateActorSeeksNeverSnapsPieData::RunCount];
	FDelegateHandle DeathHandle;
	uint32 Seq = 0x02250000u;
	double Mark = 0.0;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(StealthAssassinationPie::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
		UStealthSettings* Settings = GetMutableDefault<UStealthSettings>();
		PreviousNpcCount = Settings->NpcCount;
		Settings->NpcCount = 0;
		bPreviousFixedFrameRate = GEngine->bUseFixedFrameRate;
		PreviousFixedFrameRate = GEngine->FixedFrameRate;
		GEngine->bUseFixedFrameRate = true;
		GEngine->FixedFrameRate = StealthReopenPie::FixedFrameRate;
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
		GEngine->bUseFixedFrameRate = bPreviousFixedFrameRate;
		GEngine->FixedFrameRate = PreviousFixedFrameRate;
	}

	BEFORE_EACH()
	{
		StealthRelevancyDenySeam::ReleaseAll();
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(2).WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	AFTER_EACH()
	{
		StealthRelevancyDenySeam::ReleaseAll();
		for (LateActorSeeksNeverSnapsPieData::FRun& Run : Runs)
		{
			Run.Client.Stop();
			Run.ServerBones.Stop();
		}
		const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(StealthAssassinationPie::ServerWorld());
		UEncounterService* Service = Root != nullptr ? Root->GetEncounterService() : nullptr;
		if (FOnDeathWriteApplied* Death = Service != nullptr ? Service->GetOnDeathWriteApplied() : nullptr)
		{
			Death->Remove(DeathHandle);
		}
	}

	AStealthPawn* PawnOf(const TWeakObjectPtr<AStealthPlayerController>& Controller) const
	{
		return Controller.IsValid() ? Cast<AStealthPawn>(Controller->GetPawn()) : nullptr;
	}

	UWorld* MachineWorld(int32 Machine) const
	{
		const TArray<UWorld*> All = StealthAssassinationPie::Worlds();
		return All.IsValidIndex(Machine) ? All[Machine] : nullptr;
	}

	void Hold(const TCHAR* Label, double Seconds, TFunction<bool()> Also = nullptr)
	{
		Network
			.Then(*FString::Printf(TEXT("%s: mark"), Label), [this]() { Mark = StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()); })
			.Until(Label, [this, Seconds, Also]()
			{
				return StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()) >= Mark + Seconds && (!Also || Also());
			});
	}

	void RunLate(int32 Index)
	{
		using namespace LateActorSeeksNeverSnapsPieData;
		Network.Then(*FString::Printf(TEXT("%s: spawn an AI-driven NPC ahead of S"), Name(Index)), [this, Index]()
		{
			Runs[Index].Victim = StealthReopenPie::SpawnNpcAheadAside(PawnOf(Striker), StrikeDistanceCm);
			Runs[Index].VictimKey = Runs[Index].Victim.Get();
			ASSERT_THAT(IsTrue(Runs[Index].Victim.IsValid(), *FString::Printf(TEXT("%s: the NPC spawned"), Name(Index))));
		});
		Hold(*FString::Printf(TEXT("%s: the NPC replicates"), Name(Index)), 0.5);
		Network.Then(*FString::Printf(TEXT("%s: deny the NPC to the bystander, watch both ends"), Name(Index)), [this, Index]()
		{
			FRun& Run = Runs[Index];
			if (!Run.Victim.IsValid())
			{
				return;
			}
			Run.Client.Start(MachineWorld(BystanderMachine), StealthReopenPie::GuidOf(Run.Victim.Get()));
			Run.ServerBones.Start(Run.Victim.Get());
			Run.Keys.Reset();
			for (UWorld* World : StealthAssassinationPie::Worlds())
			{
				Run.Keys.Add(StealthPresentationPie::CopyOf(Run.Victim.Get(), World));
			}
			StealthRelevancyDenySeam::Deny(Run.Victim.Get(), Bystander.Get(), StealthRelevancyDenySeam::ERelease::Manual);
		});
		Hold(*FString::Printf(TEXT("%s: the deny settles"), Name(Index)), StealthReopenPie::DenySettleSec);
		Network.Then(*FString::Printf(TEXT("%s: strike"), Name(Index)), [this, Index]()
		{
			FRun& Run = Runs[Index];
			AStealthPawn* S = PawnOf(Striker);
			if (S == nullptr || !Run.Victim.IsValid())
			{
				return;
			}
			StealthAssassinationPie::PlaceStrikerFacing(S, Run.Victim.Get(), StrikeDistanceCm);
			StealthAssassinationPie::StrikeAt(S, Run.Victim.Get(), Seq);
		});
		Network.Until(*FString::Printf(TEXT("%s: release the bystander at DeathServerTime + %.1f s"), Name(Index), ReleaseAfterSec[Index]), [this, Index]()
		{
			FRun& Run = Runs[Index];
			const double Now = StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld());
			if (!Run.Victim.IsValid() && Run.DeathServerTime < 0.0)
			{
				return true; // no victim (asserted at the spawn): nothing to release
			}
			if (Run.DeathServerTime < 0.0 || Now < Run.DeathServerTime + ReleaseAfterSec[Index])
			{
				return false;
			}
			StealthRelevancyDenySeam::Release(Run.Victim.Get());
			Run.ReleaseServerTime = Now;
			Run.bReleased = true;
			return true;
		});
		Hold(*FString::Printf(TEXT("%s: watch the bystander"), Name(Index)), WatchAfterReleaseSec[Index]);
	}

	TEST_METHOD(MidFallSeeksWithoutPopAndPastLingerNeverShows)
	{
		using namespace StealthAssassinationPie;
		using namespace LateActorSeeksNeverSnapsPieData;
		Network
			.Until(TEXT("the round is Active and the players split 2v1"), [this]()
			{
				AStealthPlayerController* S = nullptr;
				AStealthPlayerController* P[2] = { nullptr, nullptr };
				if (!RoundActive() || !SplitTeams(S, P) || PawnOf(S) == nullptr)
				{
					return false;
				}
				Striker = S;
				// The bystander is a client pair member; the third machine is the remaining one (the host or a client).
				Bystander = P[0]->IsLocalController() ? P[1] : P[0];
				BystanderMachine = StealthPresentationPie::MachineOf(Bystander.Get());
				ThirdMachine = INDEX_NONE;
				for (int32 Machine = 0; Machine < Worlds().Num(); ++Machine)
				{
					ThirdMachine = Machine != BystanderMachine && Machine != StealthPresentationPie::MachineOf(S) ? Machine : ThirdMachine;
				}
				return BystanderMachine != INDEX_NONE && ThirdMachine != INDEX_NONE && !Bystander->IsLocalController();
			})
			.Then(TEXT("place S clear of the bushes, the pair aside; hook the death write"), [this]()
			{
				MoveClearOfBushes(PawnOf(Striker));
				const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(ServerWorld());
				UEncounterService* Service = Root != nullptr ? Root->GetEncounterService() : nullptr;
				if (FOnDeathWriteApplied* Death = Service != nullptr ? Service->GetOnDeathWriteApplied() : nullptr)
				{
					DeathHandle = Death->AddLambda([this](APawn* Victim, double DeathServerTime, bool)
					{
						for (FRun& Run : Runs)
						{
							if (Run.Victim.Get() == Victim && Run.DeathServerTime < 0.0)
							{
								Run.DeathServerTime = DeathServerTime;
								// The striker's own write of this strike (its pose holds until the lockout ends).
								const AStealthPawn* S = PawnOf(Striker);
								Run.ContactServerTime = S != nullptr && S->GetActionState().Pose == EActionPose::Strike ? S->GetActionState().ContactServerTime : -1.0;
							}
						}
					});
				}
			});
		Hold(TEXT("settle"), 0.6);
		RunLate(MidFall);
		Hold(TEXT("the striker's lockout ends"), 0.5);
		RunLate(PastLinger);

		Network.Then(TEXT("assert"), [this]()
		{
			using namespace StealthReopenPie;
			const FEncounterPresentationParams Params = FEncounterPresentationParams::FromPublic(FAssassinationTuning_Public());
			UWorld* B = MachineWorld(BystanderMachine);
			const UEncounterPresentationSubsystem* Presentation = StealthPresentationPie::PresentationOf(B);
			ASSERT_THAT(IsTrue(Presentation != nullptr, TEXT("the bystander has a presentation subsystem")));
			const double Rate = Presentation->GetClipTimings().FallLengthSec / Params.DeathFallSec;
			const double Frame = 1.0 / FixedFrameRate;

			// --- Run 1: + 0.6 s -------------------------------------------------------------------------------------
			const FRun& Mid = Runs[MidFall];
			ASSERT_THAT(IsTrue(Mid.DeathServerTime > 0.0 && Mid.bReleased, TEXT("+0.6 s: killed and released")));
			const TArray<const FInstance*> MidInstances = Mid.Client.CreatedBetween(0, MAX_uint64);
			const FInstance* Late = MidInstances.Num() > 0 ? MidInstances.Last() : nullptr;
			ASSERT_THAT(IsTrue(Late != nullptr && Late->FirstServerNow >= Mid.ReleaseServerTime - ClockToleranceSec,
				TEXT("+0.6 s: the bystander first holds the re-opened instance after the release")));
			ASSERT_THAT(IsTrue(Late->bDeathAtFirstSight, TEXT("+0.6 s: FDeathState in the initial block")));
			const FEncounterPresentationLogRow* Fall = FirstClipRow(B, Late->Key);
			ASSERT_THAT(IsTrue(Fall != nullptr && Fall->Clip == EEncounterClip::DeathFall && Fall->bLate, TEXT("+0.6 s: the first clip is Death_Fall, late")));
			const double Since = Fall->ServerNow - Mid.DeathServerTime;
			const double Seek = Fall->Value / Rate;
			TestRunner->AddInfo(FString::Printf(TEXT("ASN25 +0.6 s: released at +%.3f, instance at +%.3f, Death_Fall at +%.3f sought to %.4f s, blend %.3f s"),
				Mid.ReleaseServerTime - Mid.DeathServerTime, Late->FirstServerNow - Mid.DeathServerTime, Since, Seek, Fall->BlendInSec));
			const double Released = Mid.ReleaseServerTime - Mid.DeathServerTime;
			ASSERT_THAT(IsTrue(Since >= Released - ClockToleranceSec && Since <= Released + DeliverySec && FMath::Abs(Seek - Since) <= Frame,
				TEXT("+0.6 s: the fall sought to EstimatedServerNow() - DeathServerTime (about 0.6 s, +- 1 frame)")));
			ASSERT_THAT(IsTrue(Fall->BlendInSec > 0.0 && Fall->BlendInSec <= 0.1 + 1e-6, TEXT("+0.6 s: cross-faded within 0.1 s")));

			// No pose pop: the bystander's first shown frames against the clip's own motion plus the blend.
			TArray<const FFrameSample*> Shown;
			for (const FFrameSample& S : Late->Samples)
			{
				if (S.bShown && !S.Bones.IsEmpty())
				{
					Shown.Add(&S);
				}
			}
			FString WarmWhy;
			const bool bWarm = WarmedUpBeforeShown(B, *Late, WarmWhy);
			TestRunner->AddInfo(FString::Printf(TEXT("ASN25 +0.6 s warm-up: %s"), *WarmWhy));
			ASSERT_THAT(IsTrue(bWarm, TEXT("+0.6 s: hidden from its creation until its first posed frame (S10-R5 warm-up), never a failsafe")));
			ASSERT_THAT(IsTrue(Shown.Num() >= PopFrames, TEXT("+0.6 s: the bystander shows the body and its bones are sampled")));
			ASSERT_THAT(IsTrue(Shown[0]->Frame - Late->FirstFrame <= static_cast<uint64>(UEncounterPresentationSettings::Get()->WarmUpMaxFrames),
				TEXT("+0.6 s: shown within the warm-up (no added latency beyond WarmUpMaxFrames)")));
			double ByStanderMax = 0.0;
			for (int32 Index = 1; Index < PopFrames; ++Index)
			{
				ASSERT_THAT(IsTrue(Shown[Index]->Frame == Shown[Index - 1]->Frame + 1, TEXT("+0.6 s: the first shown frames are consecutive")));
				ByStanderMax = FMath::Max(ByStanderMax, MaxBoneDelta(Shown[Index - 1]->Bones, Shown[Index]->Bones));
			}
			const TArray<FFrameSample>& Server = Mid.ServerBones.Samples;
			const FFrameSample* Standing = nullptr;
			double ClipMax = 0.0;
			double BlendSpan = 0.0;
			for (int32 Index = 0; Index < Server.Num(); ++Index)
			{
				const FFrameSample& S = Server[Index];
				const double T = S.ServerNow - Mid.DeathServerTime;
				Standing = T < 0.0 && !S.Bones.IsEmpty() ? &S : Standing;
				if (Index > 0 && T > 0.0 && T <= Params.DeathFallSec && Server[Index - 1].ServerNow >= Mid.DeathServerTime)
				{
					ClipMax = FMath::Max(ClipMax, MaxBoneDelta(Server[Index - 1].Bones, S.Bones));
				}
			}
			for (const FFrameSample& S : Server)
			{
				const double T = S.ServerNow - Mid.DeathServerTime;
				if (Standing != nullptr && T >= Since - Frame && T <= Since + Fall->BlendInSec + Frame)
				{
					BlendSpan = FMath::Max(BlendSpan, MaxBoneDelta(Standing->Bones, S.Bones));
				}
			}
			const double Bound = ClipMax + BlendSpan * Frame / FMath::Max(Fall->BlendInSec, Frame) + PopToleranceCm;
			TestRunner->AddInfo(FString::Printf(TEXT("ASN25 +0.6 s pop: bystander max per-frame bone delta %.2f cm over %d shown frames; clip's own %.2f cm, blend span %.2f cm, bound %.2f cm"),
				ByStanderMax, PopFrames, ClipMax, BlendSpan, Bound));
			ASSERT_THAT(IsTrue(Standing != nullptr && ClipMax > 0.0, TEXT("+0.6 s: the server reference (standing pose and the fall's own motion) was sampled")));
			ASSERT_THAT(IsTrue(ByStanderMax <= Bound, TEXT("+0.6 s: no pose pop (per-frame bone delta within the clip's own motion plus the blend allowance)")));

			// --- Run 2: + 7.5 s -------------------------------------------------------------------------------------
			const FRun& Past = Runs[PastLinger];
			ASSERT_THAT(IsTrue(Past.DeathServerTime > 0.0 && Past.bReleased, TEXT("+7.5 s: killed and released")));
			const TArray<const FInstance*> PastInstances = Past.Client.CreatedBetween(0, MAX_uint64);
			const FInstance* VeryLate = PastInstances.Num() > 0 ? PastInstances.Last() : nullptr;
			ASSERT_THAT(IsTrue(VeryLate != nullptr && VeryLate->FirstServerNow >= Past.ReleaseServerTime - ClockToleranceSec,
				TEXT("+7.5 s: the bystander receives the re-opened instance after the release")));
			ASSERT_THAT(IsTrue(VeryLate->bDeathAtFirstSight, TEXT("+7.5 s: FDeathState in the initial block")));
			TestRunner->AddInfo(FString::Printf(TEXT("ASN25 +7.5 s: instance at +%.3f, %d frames sampled, shown on any: %d"),
				VeryLate->FirstServerNow - Past.DeathServerTime, VeryLate->Samples.Num(), VeryLate->EverShown() ? 1 : 0));
			ASSERT_THAT(IsTrue(VeryLate->Samples.Num() > 0 && !VeryLate->EverShown(), TEXT("+7.5 s: the body is never shown")));
			const FEncounterPresentationLogRow* Hidden = StealthPresentationPie::FirstRow(B, VeryLate->Key, EEncounterPresentationEvent::BodyHidden);
			ASSERT_THAT(IsTrue(Hidden != nullptr && Hidden->Frame == VeryLate->FirstFrame, TEXT("+7.5 s: hidden in its first frame (VA-ASN-2 NeverShow)")));
			ASSERT_THAT(IsTrue(FirstClipRow(B, VeryLate->Key) == nullptr, TEXT("+7.5 s: no clip commanded")));

			// --- The host's schedule ----------------------------------------------------------------------------------
			ASSERT_THAT(IsTrue(Mid.ContactServerTime > 0.0 && FMath::Abs(Mid.DeathServerTime - Mid.ContactServerTime) <= 1e-6,
				TEXT("host: the victim's DeathServerTime is the strike's ContactServerTime")));
			for (int32 Machine = 0; Machine < Mid.Keys.Num(); ++Machine)
			{
				if (Machine == BystanderMachine)
				{
					continue; // the late one, asserted above
				}
				UWorld* World = MachineWorld(Machine);
				const AStealthPawn* Key = Mid.Keys[Machine]; // recorded while the body still existed
				const FEncounterPresentationLogRow* Row = FirstFallRow(World, Key);
				const double Offset = Row != nullptr ? Row->ServerNow - Mid.ContactServerTime : -1.0;
				TestRunner->AddInfo(FString::Printf(TEXT("ASN25 machine %d (%s): Death_Fall at ContactServerTime %+.4f s"), Machine, Machine == 0 ? TEXT("host") : TEXT("client"), Offset));
				ASSERT_THAT(IsTrue(Row != nullptr && Offset >= -0.001 && Offset <= Frame + 0.003,
					*FString::Printf(TEXT("machine %d: Death_Fall starts at ContactServerTime (+- 1 frame), the same schedule on the host and the clients"), Machine)));
			}
		});
	}
};

#endif
