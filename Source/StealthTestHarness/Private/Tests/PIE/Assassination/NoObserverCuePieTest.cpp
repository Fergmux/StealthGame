// Assassination story asn-017 AC2 / AC-ASN-35 (ASN-R20, R23; ADR-0008 T2; listen server + 3 clients on L_Prototype,
// NpcCount 0): StealthGame.Assassination.PIE.NoObserverCue.<method>. Four players split 2v2: team A {A, AA}, team B {V, VA}.
// The game has two teams, so the AC's observers map onto them: the victim's ally, the attacker's ally and the rival
// bystander (an opponent of the attacker who is not the victim) are AA / VA by event, and the listen host is whichever
// player it serves (its leg runs the same gating, ASN-R23). In order:
//   1 a whiff: A strikes an empty spot                  observers AA, V, VA
//   2 a kill: A kills V                                 observers AA, VA (VA is V's ally)
//   3 a reversal: VA braces, A strikes VA (A dies)      observers AA (A's ally), V
//   4 the deciding strike: VA kills AA (team A is out)  observers A, V
// From each execution until its body's removal (DeathServerTime + DeathFallSec + BodyLingerSec + BodyRemovalGraceSec; a
// whiff: its lockout) every observer machine's presentation log (PresentationLogHook) has no HUD view-model change, no
// widget-list change, no camera grade or role call caused by the strike, except the dead player's allies' teammate marker
// (TM-R6 / F6); and on every observer's copies of the two participants the only strike-related replicated changes are
// FActionState, FDeathState (the Counter pose is an FActionState), movement, and the body release that follows the death
// write (PlayerState / Owner / Instigator, and the prototype bDying and brace-tell crouch until story asn-027 retires them).
// The round end (step 4): no machine presents it before the deciding death write (DeathServerTime - StrikeNetLeadSec - one
// frame); the attacker's not before its own plate, the eliminated victim's not before its own fall (HUD F10 floors).
// No owner audio bus or haptics exist yet: nothing to record (stated in PresentationLogHook.h).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/OwnPresentationPieSupport.h"
#include "Tests/PIE/Assassination/PresentationLogHook.h"
#include "View/StealthTuningPublicSubstructs.h"

namespace NoObserverCuePieData
{
	constexpr double StrikeDistanceCm = 150.0;
	constexpr double ObserverRingCm = 800.0;
	constexpr double SettleSec = 0.6;
	/** A whiff's window: the attacker's lockout (AttackAttemptLockoutSec 1.75 s) and margin. */
	constexpr double WhiffWindowSec = 2.2;
	/** The deciding strike's window ends with the round's presentation, this long after its death. */
	constexpr double RoundEndWindowSec = 2.0;
	/** The reversal's brace is up this long before the strike. */
	constexpr double BraceLeadSec = 0.4;
	/** A client's server-time estimate against the server's own clock (the ADR-0002 section 9 stand-in's accuracy). */
	constexpr double ClockToleranceSec = 0.05;

	enum EPlayer : int32 { A, AA, V, VA, PlayerCount };
	const TCHAR* PlayerName(int32 Player)
	{
		const TCHAR* const Names[PlayerCount] = { TEXT("A"), TEXT("AA"), TEXT("V"), TEXT("VA") };
		return Names[Player];
	}

	/** One scripted event. */
	struct FCueEvent
	{
		const TCHAR* Name = TEXT("");
		int32 Attacker = A;
		/** INDEX_NONE for a whiff. */
		int32 Target = INDEX_NONE;
		bool bBraced = false;
		/** The players watching (everyone else, the informants included). */
		TArray<int32> Observers;
	};

	/** The properties a strike may change on an observer's copy (beyond movement). */
	bool IsAllowedStrikeProperty(const FString& Property)
	{
		// bIsCrouched: the prototype brace tell (a crouch), ended with the Counter at its contact; story asn-027 retires it.
		static const TCHAR* const Allowed[] = { TEXT(".ActionState"), TEXT(".DeathState"), TEXT(".PlayerState"), TEXT(".Owner"), TEXT(".Instigator"), TEXT(".bDying"),
			TEXT(".bIsCrouched") };
		for (const TCHAR* Suffix : Allowed)
		{
			if (Property.EndsWith(Suffix))
			{
				return true;
			}
		}
		return StealthPresentationLog::FReplicatedPropertyWatcher::IsMovementProperty(Property);
	}
}

NETWORK_TEST_CLASS(NoObserverCue, "StealthGame.Assassination.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TWeakObjectPtr<AStealthPlayerController> Players[NoObserverCuePieData::PlayerCount];
	int32 Machines[NoObserverCuePieData::PlayerCount] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };
	TArray<NoObserverCuePieData::FCueEvent> Events;
	/** Per event and observer: the recorder, and the watchers of the attacker's and the target's copies. */
	TArray<StealthPresentationLog::FPresentationLogHook> Hooks;
	TArray<StealthPresentationLog::FReplicatedPropertyWatcher> Watchers;
	TArray<double> Issue;
	TArray<double> Exec;
	TArray<double> Death;
	/** The server time the death write was first seen (the staged write's tick, DeathServerTime - StrikeNetLeadSec). */
	TArray<double> DeathWriteAt;
	TArray<double> WindowEnd;
	StealthOwnPresentationPie::FFrameTracker Frames;
	uint32 Seq = 0x01710000u;
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
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(3).WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	AStealthPawn* PawnOf(int32 Player) const
	{
		return Players[Player].IsValid() ? Cast<AStealthPawn>(Players[Player]->GetPawn()) : nullptr;
	}

	FString NameOf(int32 Player) const
	{
		const APlayerState* State = Players[Player].IsValid() ? Players[Player]->PlayerState.Get() : nullptr;
		return State != nullptr ? State->GetPlayerName() : FString();
	}

	/** The 2v2 split: team of the first player = team A. */
	bool Split()
	{
		using namespace NoObserverCuePieData;
		const TArray<AStealthPlayerController*> All = StealthAssassinationPie::ServerPlayers();
		if (All.Num() != 4)
		{
			return false;
		}
		TArray<AStealthPlayerController*> TeamA;
		TArray<AStealthPlayerController*> TeamB;
		for (AStealthPlayerController* Controller : All)
		{
			(StealthAssassinationPie::TeamOf(Controller) == StealthAssassinationPie::TeamOf(All[0]) ? TeamA : TeamB).Add(Controller);
		}
		if (TeamA.Num() != 2 || TeamB.Num() != 2)
		{
			return false;
		}
		Players[A] = TeamA[0];
		Players[AA] = TeamA[1];
		Players[V] = TeamB[0];
		Players[VA] = TeamB[1];
		for (int32 Player = 0; Player < PlayerCount; ++Player)
		{
			Machines[Player] = StealthPresentationPie::MachineOf(Players[Player].Get());
			if (Machines[Player] == INDEX_NONE)
			{
				return false;
			}
		}
		return true;
	}

	/** The dead player's allies may see its teammate marker change (TM-R6 / F6). */
	bool IsAllowedEntry(const StealthPresentationLog::FEntry& Entry, int32 Observer, int32 Dead) const
	{
		if (Entry.Source.StartsWith(TEXT("Widget.")) && Entry.Key.StartsWith(TEXT("TeammateMarker:")) && Dead != INDEX_NONE
			&& StealthAssassinationPie::TeamOf(Players[Observer].Get()) == StealthAssassinationPie::TeamOf(Players[Dead].Get()))
		{
			return true;
		}
		// A HUD line that expires during the window was shown before it (not caused by the strike).
		return Entry.Source == TEXT("Widget.-") && Entry.Key.StartsWith(TEXT("Message:"));
	}

	void RunEvent(int32 Index)
	{
		using namespace StealthAssassinationPie;
		using namespace NoObserverCuePieData;
		const FCueEvent Event = Events[Index];
		Network.Then(*FString::Printf(TEXT("%s: place"), Event.Name), [this, Event]()
		{
			AStealthPawn* Striker = PawnOf(Event.Attacker);
			MoveClearOfBushes(Striker);
			if (Event.Target != INDEX_NONE)
			{
				PlaceFacing(PawnOf(Event.Target), Striker, StrikeDistanceCm);
			}
			// The observers stand on a ring with line of sight.
			for (int32 Slot = 0; Slot < Event.Observers.Num(); ++Slot)
			{
				if (AStealthPawn* Observer = PawnOf(Event.Observers[Slot]))
				{
					const FVector Offset = FRotator(0.0, 90.0 + Slot * 60.0, 0.0).RotateVector(FVector(ObserverRingCm, 0.0, 0.0));
					Observer->TeleportTo(Striker->GetActorLocation() + Offset, (-Offset).Rotation());
				}
			}
			Mark = ServerNow(ServerWorld());
		})
		.Until(*FString::Printf(TEXT("%s: settle"), Event.Name), [this]() { Frames.Sample(); return ServerNow(ServerWorld()) >= Mark + SettleSec; });
		if (Event.bBraced)
		{
			Network
				.Then(*FString::Printf(TEXT("%s: brace"), Event.Name), [this, Event]()
				{
					AsOwner(PawnOf(Event.Target), Seq, [](UPawnCommandComponent& Command) { Command.PrepareBegin(); });
					Mark = ServerNow(ServerWorld());
				})
				.Until(*FString::Printf(TEXT("%s: brace up"), Event.Name), [this]() { Frames.Sample(); return ServerNow(ServerWorld()) >= Mark + BraceLeadSec; });
		}
		Network
			.Then(*FString::Printf(TEXT("%s: record and strike"), Event.Name), [this, Index, Event]()
			{
				const TArray<UWorld*> All = Worlds();
				for (int32 Observer : Event.Observers)
				{
					StealthPresentationLog::FPresentationLogHook& Hook = Hooks.AddDefaulted_GetRef();
					Hook.Begin(Machines[Observer], [](const AStealthPawn*) { return FString(); });
					for (int32 Participant : { Event.Attacker, Event.Target })
					{
						StealthPresentationLog::FReplicatedPropertyWatcher& Watcher = Watchers.AddDefaulted_GetRef();
						const bool bRemote = Machines[Observer] > 0 && Participant != INDEX_NONE;
						Watcher.Begin(bRemote ? StealthPresentationPie::CopyOf(PawnOf(Participant), All[Machines[Observer]]) : nullptr, Machines[Observer]);
					}
				}
				AStealthPawn* Striker = PawnOf(Event.Attacker);
				if (Event.Target != INDEX_NONE)
				{
					StrikeAt(Striker, PawnOf(Event.Target), Seq);
				}
				else
				{
					FAttackPayload Payload;
					Payload.WorldPoint = Striker->GetActorLocation() + Striker->GetActorForwardVector() * StrikeDistanceCm;
					AsOwner(Striker, Seq, [&Payload](UPawnCommandComponent& Command) { Command.Attack(Payload); });
				}
				Issue[Index] = ServerNow(ServerWorld());
				Exec[Index] = -1.0;
				Death[Index] = -1.0;
				WindowEnd[Index] = ServerNow(ServerWorld()) + WhiffWindowSec;
			})
			.Until(*FString::Printf(TEXT("%s: record to the body's removal"), Event.Name), [this, Index, Event]()
			{
				Frames.Sample();
				for (StealthPresentationLog::FPresentationLogHook& Hook : Hooks)
				{
					Hook.Sample();
				}
				for (StealthPresentationLog::FReplicatedPropertyWatcher& Watcher : Watchers)
				{
					Watcher.Sample();
				}
				const AStealthPawn* Striker = PawnOf(Event.Attacker);
				if (Exec[Index] < 0.0 && Striker != nullptr && Striker->GetActionState().Pose == EActionPose::Strike && Striker->GetActionState().StartServerTime + 0.001 >= Issue[Index])
				{
					Exec[Index] = Striker->GetActionState().StartServerTime;
				}
				for (int32 Participant : { Event.Attacker, Event.Target })
				{
					const AStealthPawn* Pawn = Participant != INDEX_NONE ? PawnOf(Participant) : nullptr;
					if (Death[Index] < 0.0 && Pawn != nullptr && Pawn->HasDeathWrite() && Pawn->GetDeathState().DeathServerTime >= Issue[Index])
					{
						Death[Index] = Pawn->GetDeathState().DeathServerTime;
						DeathWriteAt[Index] = ServerNow(ServerWorld());
						const FAssassinationTuning_Public Tuning;
						WindowEnd[Index] = Index == Events.Num() - 1 ? Death[Index] + RoundEndWindowSec
							: Death[Index] + Tuning.DeathFallSec + Tuning.BodyLingerSec + Tuning.BodyRemovalGraceSec;
					}
				}
				return ServerNow(ServerWorld()) >= WindowEnd[Index];
			});
	}

	TEST_METHOD(ObserversSeeNoCueForAKillAWhiffOrAReversalAndNoEarlyRoundEnd)
	{
		using namespace StealthAssassinationPie;
		using namespace NoObserverCuePieData;
		Events = {
			{ TEXT("1 whiff"), A, INDEX_NONE, false, { AA, V, VA } },
			{ TEXT("2 kill"), A, V, false, { AA, VA } },
			{ TEXT("3 reversal"), A, VA, true, { AA, V } },
			{ TEXT("4 deciding strike"), VA, AA, false, { A, V } },
		};
		Issue.Init(-1.0, Events.Num());
		Exec.Init(-1.0, Events.Num());
		Death.Init(-1.0, Events.Num());
		DeathWriteAt.Init(-1.0, Events.Num());
		WindowEnd.Init(-1.0, Events.Num());
		Network.Until(TEXT("the round is Active and the players split 2v2"), [this]() { return RoundActive() && Split(); });
		for (int32 Index = 0; Index < Events.Num(); ++Index)
		{
			RunEvent(Index);
		}

		Network.Then(TEXT("assert"), [this]()
		{
			using namespace StealthPresentationLog;
			// Each machine's round-end presentation: what follows it is the round end's (checked below), not a strike cue.
			auto RoundEndOf = [](int32 Machine)
			{
				const AStealthPlayerController* Local = StealthOwnPresentationPie::LocalOf(Machine);
				const FOwnPresentationRecord* Record = Local != nullptr ? Local->GetOwnPresentationLog().FindByPredicate([](const FOwnPresentationRecord& R) { return R.Event == EOwnPresentationEvent::RoundEnd; }) : nullptr;
				return Record != nullptr ? Record->ServerNow : TNumericLimits<double>::Max();
			};
			int32 HookAt = 0;
			int32 WatcherAt = 0;
			for (int32 Index = 0; Index < Events.Num(); ++Index)
			{
				const FCueEvent& Event = Events[Index];
				// The dead player of the event: the target of a kill, the attacker of a reversal.
				const int32 Dead = Event.Target == INDEX_NONE ? INDEX_NONE : (Event.bBraced ? Event.Attacker : Event.Target);
				ASSERT_THAT(IsTrue(Exec[Index] > 0.0, *FString::Printf(TEXT("%s executed"), Event.Name)));
				ASSERT_THAT(IsTrue(Event.Target == INDEX_NONE ? Death[Index] < 0.0 : Death[Index] > 0.0, *FString::Printf(TEXT("%s: a death write exactly when a body met a body"), Event.Name)));
				const bool bLast = Index == Events.Num() - 1;
				for (int32 Observer : Event.Observers)
				{
					const FPresentationLogHook& Hook = Hooks[HookAt++];
					TArray<FString> Cues;
					for (const FEntry& Entry : Hook.Collect())
					{
						const bool bInWindow = Entry.ServerNow + 0.001 >= Exec[Index] && Entry.ServerNow < WindowEnd[Index];
						const bool bRoundEnd = bLast && (Entry.ServerNow + 0.001 >= RoundEndOf(Machines[Observer]) || Entry.Key.Contains(TEXT(" win: "))
							|| Entry.Key.StartsWith(TEXT("RoundBanner:")) || Entry.Source == TEXT("Hud.RoundEnd"));
						if (bInWindow && !bRoundEnd && (Entry.Source.StartsWith(TEXT("Hud.")) || Entry.Source.StartsWith(TEXT("Widget.")) || Entry.Source.StartsWith(TEXT("Camera.")))
							&& !IsAllowedEntry(Entry, Observer, Dead))
						{
							Cues.Add(FString::Printf(TEXT("%+.3f %s %s"), Entry.ServerNow - Exec[Index], *Entry.Source, *Entry.Key));
						}
					}
					TArray<FString> Changes;
					for (int32 Copy = 0; Copy < 2; ++Copy)
					{
						for (const FPropertyChange& Change : Watchers[WatcherAt++].GetChanges())
						{
							// The deciding strike's round end (the prototype holds every pawn at the round-end step = its death write) is
							// the round-end clause's, checked below.
							const bool bRoundEndChange = bLast && Change.ServerNow + Frames.FrameOf(Machines[Observer]) + ClockToleranceSec >= DeathWriteAt[Index];
							if (Change.ServerNow + 0.001 >= Exec[Index] && Change.ServerNow < WindowEnd[Index] && !bRoundEndChange && !IsAllowedStrikeProperty(Change.Property))
							{
								Changes.Add(FString::Printf(TEXT("%s %+.3f %s"), Copy == 0 ? TEXT("attacker") : TEXT("target"), Change.ServerNow - Exec[Index], *Change.Property));
							}
						}
					}
					TestRunner->AddInfo(FString::Printf(TEXT("ASN35 %s observer %s (machine %d%s): cues [%s]; other replicated changes [%s]"), Event.Name, PlayerName(Observer),
						Machines[Observer], Machines[Observer] == 0 ? TEXT(", listen host") : TEXT(""), *FString::Join(Cues, TEXT("; ")), *FString::Join(Changes, TEXT("; "))));
					ASSERT_THAT(IsTrue(Cues.IsEmpty(), *FString::Printf(TEXT("%s: no widget, HUD, camera or marker cue for observer %s"), Event.Name, PlayerName(Observer))));
					ASSERT_THAT(IsTrue(Changes.IsEmpty(), *FString::Printf(TEXT("%s: observer %s receives only FActionState, FDeathState, movement and the body release"), Event.Name, PlayerName(Observer))));
				}
			}

			// The round end of the deciding strike (step 4): staged to its death write; F10 floors for the two participants.
			const int32 Last = Events.Num() - 1;
			for (int32 Player = 0; Player < PlayerCount; ++Player)
			{
				const int32 Machine = Machines[Player];
				const double Frame = Frames.FrameOf(Machine);
				double RoundEnd = -1.0;
				double Banner = -1.0;
				double Plate = -1.0;
				if (const AStealthPlayerController* Local = StealthOwnPresentationPie::LocalOf(Machine))
				{
					for (const FOwnPresentationRecord& Record : Local->GetOwnPresentationLog())
					{
						if (Record.ServerNow + 0.001 < Exec[Last])
						{
							continue;
						}
						RoundEnd = RoundEnd < 0.0 && Record.Event == EOwnPresentationEvent::RoundEnd ? Record.ServerNow : RoundEnd;
						Banner = Banner < 0.0 && Record.Event == EOwnPresentationEvent::HudMessage && Record.Text.Contains(TEXT(" win: ")) ? Record.ServerNow : Banner;
						Plate = Plate < 0.0 && Record.Event == EOwnPresentationEvent::ResultPlate ? Record.ServerNow : Plate;
					}
				}
				// The deciding death write as the server ran it (DeathServerTime - StrikeNetLeadSec), less one frame and the client
				// estimate's accuracy (the round end is stamped with that machine's EstimatedServerNow()).
				double Floor = DeathWriteAt[Last] - Frame - ClockToleranceSec;
				const TCHAR* Rule = TEXT("the deciding death write");
				if (Player == Events[Last].Attacker)
				{
					Floor = FMath::Max(Floor, Plate - 0.001);
					Rule = TEXT("own plate");
				}
				else if (Player == Events[Last].Target)
				{
					Floor = FMath::Max(Floor, Death[Last] - 0.001);
					Rule = TEXT("own fall at DeathServerTime");
				}
				TestRunner->AddInfo(FString::Printf(TEXT("ASN35 round end on %s (machine %d): presented %.3f, line %.3f; DeathServerTime %.3f (death write at %.3f), own plate %.3f; floor %.3f (%s)"),
					PlayerName(Player), Machine, RoundEnd, Banner, Death[Last], DeathWriteAt[Last], Plate, Floor, Rule));
				ASSERT_THAT(IsTrue(RoundEnd > 0.0 && RoundEnd >= Floor, *FString::Printf(TEXT("round end on %s's machine not before its floor (%s)"), PlayerName(Player), Rule)));
				ASSERT_THAT(IsTrue(Banner < 0.0 || Banner >= Floor, *FString::Printf(TEXT("the round-end line on %s's machine not before its floor (%s)"), PlayerName(Player), Rule)));
				if (Player == Events[Last].Attacker)
				{
					ASSERT_THAT(IsTrue(Plate > 0.0, TEXT("the deciding attacker's plate was shown")));
				}
			}
		});
	}
};

#endif
