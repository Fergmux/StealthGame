// NPC-Kill Penalty story nkp-014, AC2 / AC-NKP-37 (PEN-R9, IVP R9c, R14; ADR-0019 section 4; ADR-0008 T2), the clause that
// can be proven in process today: StealthGame.Penalty.PIE.TakeoverWireParity.NoChannelOpenOrCloseAndSameNetGuid. Listen
// server + 2 clients on L_Prototype WITH a crowd, under the LeakNetProfile (PacketSimulationProfile StealthLeak1: PktLag 100,
// PktLoss 1). The solo player (a client: the non-team observer) kills a pair player; when its informant takes over a crowd
// pawn relevant to the observer, the observer's copy of that pawn is the SAME actor object on the SAME actor channel (same
// channel object and index) with the SAME NetGUID before and 5 s after the takeover: no actor channel opened or closed and
// no actor, component or NetGUID was created for the takeover.
// Measured and logged, NOT asserted (story nkp-014 AC2 is Blocked on their owners): the replicated non-movement property
// changes of the taken pawn against a control NPC on the observer (the owner-only PlayerState / Controller / Owner reset
// on AStealthPawn is information-visibility-policy story 018, Blocked; RemoteViewPitch16 from HeadLook, the sample's
// CharacterInputState and TraversalResult are shared-character-locomotion's pawn differentiators, spike S1), and the bunch
// cadence (needs the ADR-0008 section 6 raw-bunch capture, multi-process).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "Engine/ActorChannel.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "HAL/IConsoleManager.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Penalty/PenaltyPieSupport.h"
#include "UObject/UnrealType.h"

namespace TakeoverWireParityPieData
{
	/** The window after the takeover (AC-NKP-37: 5 s). */
	constexpr double WindowSec = 5.0;
	/** The observer stands this far from the victim team's anchor (inside the default pawn net cull distance of the region). */
	constexpr double ObserverFromAnchorCm = 3000.0;
	/** The ADR-0008 LeakNetProfile (Config/DefaultEngine.ini [PacketSimulationProfile.StealthLeak1]). */
	const TCHAR* const LeakNetProfile = TEXT("StealthLeak1");

	/** Replicated property names that carry movement (excluded from the content measurement, as AC-NKP-37 says). */
	const TCHAR* const MovementNames[] = { TEXT("ReplicatedMovement"), TEXT("ReplicatedBasedMovement"), TEXT("ReplicatedServerLastTransformUpdateTimeStamp"),
		TEXT("ReplicatedMovementMode"), TEXT("ReplicatedGravityDirection"), TEXT("RepRootMotion"), TEXT("bProxyIsJumpForceApplied"),
		TEXT("AnimRootMotionTranslationScale") };

	/** Every replicated (CPF_Net) property value of Actor and its replicated components, by "Object.Property". */
	TMap<FString, FString> Snapshot(const AActor* Actor)
	{
		TMap<FString, FString> Out;
		if (Actor == nullptr)
		{
			return Out;
		}
		const auto AddObject = [&Out](const UObject* Object, const FString& Prefix)
		{
			for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
			{
				if (!It->HasAnyPropertyFlags(CPF_Net))
				{
					continue;
				}
				bool bMovement = false;
				for (const TCHAR* Name : MovementNames)
				{
					bMovement |= It->GetName() == Name;
				}
				if (!bMovement)
				{
					FString Value;
					It->ExportTextItem_InContainer(Value, Object, nullptr, nullptr, PPF_None);
					Out.Add(Prefix + It->GetName(), Value);
				}
			}
		};
		AddObject(Actor, TEXT("Actor."));
		for (const UActorComponent* Component : Actor->GetComponents())
		{
			if (Component != nullptr && Component->GetIsReplicated())
			{
				AddObject(Component, Component->GetName() + TEXT("."));
			}
		}
		return Out;
	}

	/** The names whose value differs between Before and After. */
	FString Changed(const TMap<FString, FString>& Before, const TMap<FString, FString>& After)
	{
		FString Out;
		for (const TPair<FString, FString>& Pair : After)
		{
			const FString* Old = Before.Find(Pair.Key);
			if (Old == nullptr || *Old != Pair.Value)
			{
				Out += (Out.IsEmpty() ? TEXT("") : TEXT(", ")) + Pair.Key;
			}
		}
		return Out.IsEmpty() ? FString(TEXT("none")) : Out;
	}
}

NETWORK_TEST_CLASS(TakeoverWireParity, "StealthGame.Penalty.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;
	inline static FPacketSimulationSettings LeakSettings;

	TWeakObjectPtr<AStealthPlayerController> Solo;
	TWeakObjectPtr<AStealthPlayerController> Pair[2];
	TWeakObjectPtr<AStealthPawn> SoloPawn;
	FParticipantId Victim;
	TWeakObjectPtr<APawn> OldBody;
	uint32 Seq = 0x38000000u;
	double Mark = 0.0;

	/** Server: the taken pawn; observer: its copy, channel, channel index and NetGUID at the takeover. */
	TWeakObjectPtr<APawn> Taken;
	TWeakObjectPtr<AActor> ObservedCopy;
	TWeakObjectPtr<UActorChannel> ObservedChannel;
	int32 ObservedChIndex = INDEX_NONE;
	FNetworkGUID ObservedGuid;
	/** The control NPC (server pawn and observer copy). */
	TWeakObjectPtr<APawn> Control;
	TWeakObjectPtr<AActor> ControlCopy;
	TMap<FString, FString> TakenBefore;
	TMap<FString, FString> ControlBefore;
	double TakeoverAt = 0.0;

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
		LeakSettings = FPacketSimulationSettings();
		LeakSettings.LoadEmulationProfile(TakeoverWireParityPieData::LeakNetProfile);
	}

	AFTER_ALL()
	{
		StealthPenaltyHarnessSeams::SetRespawnProbe(false);
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
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(2).WithPacketSimulationSettings(&LeakSettings)
			.WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	double Now() const
	{
		return StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld());
	}

	AStealthPawn* PawnOf(const TWeakObjectPtr<AStealthPlayerController>& Controller) const
	{
		return Controller.IsValid() ? Cast<AStealthPawn>(Controller->GetPawn()) : nullptr;
	}

	/** The observer (the solo player's client) world's net driver. */
	UNetDriver* ObserverDriver() const
	{
		const AStealthPlayerController* Machine = StealthAssassinationPie::OwnerMachineController(Solo.Get());
		UWorld* World = Machine != nullptr ? Machine->GetWorld() : nullptr;
		return World != nullptr && World->GetNetMode() == NM_Client ? World->GetNetDriver() : nullptr;
	}

	/** The observer's copy of a server actor (matched by the shared NetGUID), or null when it has none. */
	AActor* ObserverCopy(const AActor* ServerActor, FNetworkGUID* OutGuid = nullptr) const
	{
		UWorld* Server = StealthAssassinationPie::ServerWorld();
		UNetDriver* ServerDriver = Server != nullptr ? Server->GetNetDriver() : nullptr;
		UNetDriver* Client = ObserverDriver();
		if (ServerActor == nullptr || ServerDriver == nullptr || Client == nullptr || !ServerDriver->GetNetGuidCache().IsValid() || !Client->GetNetGuidCache().IsValid())
		{
			return nullptr;
		}
		const FNetworkGUID Guid = ServerDriver->GetNetGuidCache()->GetNetGUID(ServerActor);
		if (OutGuid != nullptr)
		{
			*OutGuid = Guid;
		}
		return Guid.IsValid() ? Cast<AActor>(Client->GetNetGuidCache()->GetObjectFromNetGUID(Guid, false)) : nullptr;
	}

	UActorChannel* ObserverChannel(AActor* Copy) const
	{
		UNetDriver* Client = ObserverDriver();
		UNetConnection* Connection = Client != nullptr ? Client->ServerConnection : nullptr;
		return Connection != nullptr && Copy != nullptr ? Connection->FindActorChannelRef(Copy) : nullptr;
	}

	TEST_METHOD(NoChannelOpenOrCloseAndSameNetGuid)
	{
		using namespace StealthAssassinationPie;
		using namespace TakeoverWireParityPieData;
		Network
			.Until(TEXT("the round is Active, the crowd is up and the players split 2v1"), [this]()
			{
				AStealthPlayerController* S = nullptr;
				AStealthPlayerController* P[2] = { nullptr, nullptr };
				if (!RoundActive() || !SplitTeams(S, P) || StealthPenaltyPie::Penalty() == nullptr || StealthPenaltyPie::Roster() == nullptr)
				{
					return false;
				}
				int32 Crowd = 0;
				for (TActorIterator<AStealthPawn> It(ServerWorld()); It; ++It)
				{
					Crowd += StealthPenaltyPie::Roster()->IsPawnBound(*It) ? 0 : 1;
				}
				Solo = S;
				Pair[0] = P[0];
				Pair[1] = P[1];
				SoloPawn = Cast<AStealthPawn>(S->GetPawn());
				return SoloPawn.IsValid() && Crowd >= StealthPenaltyPie::CrowdNpcCount / 2 && OwnerMachineController(S) != nullptr;
			})
			.Then(TEXT("the observer is a client of the other team; it stands near the victim team's region; it kills a pair player"), [this]()
			{
				ASSERT_THAT(IsTrue(ObserverDriver() != nullptr, TEXT("fixture: the solo player (the non-team observer) is a client machine")));
				Victim = StealthPenaltyPie::ParticipantOf(Pair[1].Get());
				FVector Anchor;
				ASSERT_THAT(IsTrue(StealthPenaltyPie::TeamAnchor(StealthPenaltyPie::TeamOf(Victim), Anchor), TEXT("fixture: the victim's team has a spawn anchor")));
				SoloPawn->TeleportTo(Anchor + FVector(ObserverFromAnchorCm, 0.0, 0.0), SoloPawn->GetActorRotation());
				MoveClearOfBushes(SoloPawn.Get());
				OldBody = PawnOf(Pair[1]);
				StealthPenaltyHarnessSeams::SetRespawnProbe(true);
				PlaceFacing(PawnOf(Pair[1]), SoloPawn.Get(), 150.0);
				StrikeAt(SoloPawn.Get(), PawnOf(Pair[1]), Seq);
			})
			.Until(TEXT("the informant's takeover commits: record the observer's copy, channel and NetGUID"), [this]()
			{
				const StealthPenaltyHarnessSeams::FRespawnProbeEvent* Applied = StealthPenaltyHarnessSeams::GetRespawnProbeEvents().FindByPredicate(
					[](const StealthPenaltyHarnessSeams::FRespawnProbeEvent& E)
					{
						return E.Step == StealthPenaltyHarnessSeams::ERespawnProbeStep::Takeover && E.TakeoverResult == EPenaltyTakeoverResult::Applied;
					});
				if (Applied == nullptr || !Applied->Pawn.IsValid())
				{
					return false;
				}
				Taken = Applied->Pawn;
				TakeoverAt = Applied->ServerTime;
				ObservedCopy = ObserverCopy(Taken.Get(), &ObservedGuid);
				ObservedChannel = ObserverChannel(ObservedCopy.Get());
				ObservedChIndex = ObservedChannel.IsValid() ? ObservedChannel->ChIndex : INDEX_NONE;
				TakenBefore = Snapshot(ObservedCopy.Get());
				// The control: the nearest other unbound crowd pawn the observer also has.
				double Best = TNumericLimits<double>::Max();
				for (TActorIterator<AStealthPawn> It(ServerWorld()); It; ++It)
				{
					AActor* Copy = *It != Taken.Get() && !StealthPenaltyPie::Roster()->IsPawnBound(*It) ? ObserverCopy(*It) : nullptr;
					const double Distance = Copy != nullptr ? FVector::Dist(It->GetActorLocation(), Taken->GetActorLocation()) : Best;
					if (Distance < Best)
					{
						Best = Distance;
						Control = *It;
						ControlCopy = Copy;
					}
				}
				ControlBefore = Snapshot(ControlCopy.Get());
				return true;
			})
			.Then(TEXT("the taken pawn is relevant to the observer"), [this]()
			{
				ASSERT_THAT(IsTrue(ObservedCopy.IsValid() && ObservedChannel.IsValid() && ObservedGuid.IsValid(),
					TEXT("the observer has the taken pawn on an open actor channel at the takeover (relevant, unseen at commit)")));
			})
			.Until(TEXT("the 5 s after the takeover"), [this]() { return Now() >= TakeoverAt + WindowSec; })
			.Then(TEXT("assert: no actor channel opened or closed, the same actor and NetGUID"), [this]()
			{
				FNetworkGUID GuidAfter;
				AActor* CopyAfter = ObserverCopy(Taken.Get(), &GuidAfter);
				UActorChannel* ChannelAfter = ObserverChannel(CopyAfter);
				TestRunner->AddInfo(FString::Printf(TEXT("NKPPIE wire: taken %s guid %s ch %d -> copy %s guid %s ch %d; bound to the informant %d"),
					*GetNameSafe(ObservedCopy.Get()), *ObservedGuid.ToString(), ObservedChIndex, *GetNameSafe(CopyAfter), *GuidAfter.ToString(),
					ChannelAfter != nullptr ? ChannelAfter->ChIndex : -1, StealthPenaltyPie::BodyOf(Victim) == Taken.Get() ? 1 : 0));
				ASSERT_THAT(IsTrue(StealthPenaltyPie::BodyOf(Victim) == Taken.Get(), TEXT("the informant is still in the taken pawn")));
				ASSERT_THAT(IsTrue(Taken.IsValid() && GuidAfter == ObservedGuid, TEXT("the taken pawn keeps its NetGUID (none created)")));
				ASSERT_THAT(IsTrue(CopyAfter != nullptr && CopyAfter == ObservedCopy.Get(), TEXT("the observer keeps the same actor object (never closed and re-opened)")));
				ASSERT_THAT(IsTrue(ChannelAfter != nullptr && ChannelAfter == ObservedChannel.Get() && ChannelAfter->ChIndex == ObservedChIndex,
					TEXT("the observer's actor channel for it is the same channel (no actor channel opened or closed)")));

				// Measured, not asserted (AC2's content and cadence clauses are Blocked on IVP story 018 and SCL's pawn differentiators).
				TestRunner->AddInfo(FString::Printf(TEXT("NKPPIE wire: taken pawn, non-movement replicated changes on the observer: %s"), *Changed(TakenBefore, Snapshot(CopyAfter))));
				TestRunner->AddInfo(FString::Printf(TEXT("NKPPIE wire: control NPC %s, non-movement replicated changes on the observer: %s"), *GetNameSafe(ControlCopy.Get()),
					*Changed(ControlBefore, Snapshot(ControlCopy.Get()))));
			});
	}
};

#endif
