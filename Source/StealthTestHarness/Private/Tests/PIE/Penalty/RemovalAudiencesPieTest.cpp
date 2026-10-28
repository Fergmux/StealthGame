// NPC-Kill Penalty story nkp-016, AC3 / AC-NKP-21 (PEN-R15; ADR-0001 audiences; ADR-0019 sections 1-2; ADR-0008 T2):
// StealthGame.Penalty.PIE.RemovalAudiences.ExileReachesEachAudienceAsAnNpcKillOrAnAssassination. Listen server + 3 clients
// on L_Prototype with a crowd, the four players split 2v2: A (the exiled-to-be, a client machine) and its ally B; C (an
// opponent on a client machine) and its ally D.
//   1. The Assassinated reference: A kills D. C's C-TeamView entry of D is sampled every frame for the 3 s after the kill:
//      the sequence of changed-field sets of the entry (State, PawnRef, RetireAtServerTime, ...).
//   2. A kills L NPCs; the L-th exiles it. B's entry of A is sampled the same way for the 3 s after the exiling strike, and
//      C's copy of A's body (the released pawn) has its replicated non-movement properties snapshotted before and 3 s
//      after kill L-1 (an NPC kill by the same attacker, no exile) and kill L (the exile).
// Asserts:
//   - only the exiled player's machine receives bExiled (exactly one StrikeResult reply with bExiled on A's machine; none
//     on B's, C's or D's): the owner-only positive control;
//   - the ally receives only the Roster Team-audience state change, identical in shape to an Assassinated removal: the
//     sequence of changed-field sets of B's entry of A equals that of C's entry of D;
//   - the opponent keeps the same actor, actor class, component classes, actor channel and NetGUID for the released pawn
//     across the exile (classes and replication conditions are those of the class: unchanged), and its non-movement
//     replicated property changes over the exile window equal those over the NPC-kill window, EXCEPT the engine-default
//     identity fields information-visibility-policy story 018 owns (Actor.PlayerState / Owner / Instigator, the same leak
//     story nkp-014 measured on a takeover): those are logged, not asserted, and the clause is recorded Blocked on IVP 018;
//     the update cadence needs the ADR-0008 section 6 multi-process raw-bunch capture and is not measured here.
// The witness-side residual PEN-R14.2 (an opponent who strikes the released body may see WrongTarget) is accepted by the GDD
// and is not tested away. Logged lines start with "NKP016".

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
#include "Tests/PIE/Penalty/PenaltyPresentationPieSupport.h"
#include "UObject/UnrealType.h"
#include "View/StealthRosterTeamView.h"

namespace RemovalAudiencesPieData
{
	constexpr double SettleSec = 0.6;
	/** The window over which an ally's entry is sampled after a removal. */
	constexpr double EntryWindowSec = 3.0;
	/** Replicated property names that carry movement (excluded, as AC-NKP-21 says only movement may differ). */
	const TCHAR* const MovementNames[] = { TEXT("ReplicatedMovement"), TEXT("ReplicatedBasedMovement"), TEXT("ReplicatedServerLastTransformUpdateTimeStamp"),
		TEXT("ReplicatedMovementMode"), TEXT("ReplicatedGravityDirection"), TEXT("RepRootMotion"), TEXT("bProxyIsJumpForceApplied"),
		TEXT("AnimRootMotionTranslationScale") };
	/** The engine-default identity fields owned by information-visibility-policy story 018 (owner-only on AStealthPawn). */
	const TCHAR* const IdentityLeakNames[] = { TEXT("Actor.PlayerState"), TEXT("Actor.Owner"), TEXT("Actor.Instigator"), TEXT("Actor.Controller") };

	/** Every replicated non-movement property value of Actor and its replicated components, by "Object.Property". */
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

	TSet<FString> Changed(const TMap<FString, FString>& Before, const TMap<FString, FString>& After)
	{
		TSet<FString> Out;
		for (const TPair<FString, FString>& Pair : After)
		{
			const FString* Old = Before.Find(Pair.Key);
			if (Old == nullptr || *Old != Pair.Value)
			{
				Out.Add(Pair.Key);
			}
		}
		return Out;
	}

	bool IsIdentityLeakField(const FString& Name)
	{
		for (const TCHAR* Leak : IdentityLeakNames)
		{
			if (Name == Leak)
			{
				return true;
			}
		}
		return false;
	}

	FString Join(const TSet<FString>& Names)
	{
		TArray<FString> Sorted = Names.Array();
		Sorted.Sort();
		return Sorted.Num() > 0 ? FString::Join(Sorted, TEXT(", ")) : FString(TEXT("none"));
	}

	/** The changed fields between two teammate entries ("State+PawnRef+..."), empty when equal. */
	FString EntryDiff(const FTeamViewTeammateEntry& A, const FTeamViewTeammateEntry& B)
	{
		TArray<FString> Fields;
		if (A.Handle != B.Handle) { Fields.Add(TEXT("Handle")); }
		if (A.State != B.State) { Fields.Add(TEXT("State")); }
		if ((A.PawnRef == nullptr) != (B.PawnRef == nullptr) || A.PawnRef != B.PawnRef) { Fields.Add(TEXT("PawnRef")); }
		if (A.bIsConcealed != B.bIsConcealed) { Fields.Add(TEXT("bIsConcealed")); }
		if (A.RetireAtServerTime != B.RetireAtServerTime) { Fields.Add(TEXT("RetireAtServerTime")); }
		if (A.bReconnecting != B.bReconnecting) { Fields.Add(TEXT("bReconnecting")); }
		if (A.ReconnectEndCause != B.ReconnectEndCause) { Fields.Add(TEXT("ReconnectEndCause")); }
		return FString::Join(Fields, TEXT("+"));
	}

	/** The one teammate entry of a 2v2 owner view (null when there is none). */
	const FTeamViewTeammateEntry* MateOf(const AStealthPlayerController* Machine)
	{
		const URosterTeamViewComponent* View = Machine != nullptr ? Machine->GetTeamView() : nullptr;
		return View != nullptr && View->GetView().Teammates.Num() == 1 ? &View->GetView().Teammates[0] : nullptr;
	}

	/** The owner view's teammate list of Machine (empty without a view). */
	TArray<FTeamViewTeammateEntry> MatesOf(const AStealthPlayerController* Machine)
	{
		const URosterTeamViewComponent* View = Machine != nullptr ? Machine->GetTeamView() : nullptr;
		return View != nullptr ? View->GetView().Teammates : TArray<FTeamViewTeammateEntry>();
	}

	/** The change between two teammate lists: "Count:a->b" when the list changes size, else the per-entry field diffs. */
	FString ListDiff(const TArray<FTeamViewTeammateEntry>& A, const TArray<FTeamViewTeammateEntry>& B)
	{
		if (A.Num() != B.Num())
		{
			return FString::Printf(TEXT("Count:%d->%d"), A.Num(), B.Num());
		}
		TArray<FString> Parts;
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			const FString Diff = EntryDiff(A[Index], B[Index]);
			if (!Diff.IsEmpty())
			{
				Parts.Add(Diff);
			}
		}
		return FString::Join(Parts, TEXT(";"));
	}
}

NETWORK_TEST_CLASS(RemovalAudiences, "StealthGame.Penalty.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TWeakObjectPtr<AStealthPlayerController> A;
	TWeakObjectPtr<AStealthPlayerController> B;
	TWeakObjectPtr<AStealthPlayerController> C;
	TWeakObjectPtr<AStealthPlayerController> D;
	uint32 Seq = 0x54000000u;
	double Mark = 0.0;

	/** The entry-change sequences: C's entry of D after the assassination, B's entry of A after the exile. */
	TArray<FString> AssassinatedShape;
	TArray<FString> ExileShape;
	TArray<FTeamViewTeammateEntry> LastMates;
	double WindowEnd = 0.0;

	/** C's copy of A's body: before / after the NPC-kill window and the exile window. */
	TWeakObjectPtr<APawn> Body;
	TWeakObjectPtr<AActor> Copy;
	FNetworkGUID Guid;
	TWeakObjectPtr<UActorChannel> Channel;
	TMap<FString, FString> NpcBefore;
	TSet<FString> NpcChanged;
	TMap<FString, FString> ExileBefore;
	TSet<FString> ExileChanged;
	FString ComponentClassesBefore;

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
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(3).WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	double Now() const { return StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()); }

	void Wait(const TCHAR* Label, double Seconds)
	{
		Network
			.Then(*FString::Printf(TEXT("%s: mark"), Label), [this]() { Mark = Now(); })
			.Until(Label, [this, Seconds]() { return Now() >= Mark + Seconds; });
	}

	static AStealthPawn* PawnOf(const TWeakObjectPtr<AStealthPlayerController>& Controller)
	{
		return Controller.IsValid() ? Cast<AStealthPawn>(Controller->GetPawn()) : nullptr;
	}

	static AStealthPlayerController* MachineOf(const TWeakObjectPtr<AStealthPlayerController>& Controller)
	{
		return StealthAssassinationPie::OwnerMachineController(Controller.Get());
	}

	/** C's (the opponent observer's) net driver. */
	UNetDriver* ObserverDriver() const
	{
		const AStealthPlayerController* Machine = MachineOf(C);
		UWorld* World = Machine != nullptr ? Machine->GetWorld() : nullptr;
		return World != nullptr && World->GetNetMode() == NM_Client ? World->GetNetDriver() : nullptr;
	}

	AActor* ObserverCopy(const AActor* ServerActor, FNetworkGUID* OutGuid = nullptr) const
	{
		UWorld* Server = StealthAssassinationPie::ServerWorld();
		UNetDriver* ServerDriver = Server != nullptr ? Server->GetNetDriver() : nullptr;
		UNetDriver* Client = ObserverDriver();
		if (ServerActor == nullptr || ServerDriver == nullptr || Client == nullptr || !ServerDriver->GetNetGuidCache().IsValid() || !Client->GetNetGuidCache().IsValid())
		{
			return nullptr;
		}
		const FNetworkGUID Found = ServerDriver->GetNetGuidCache()->GetNetGUID(ServerActor);
		if (OutGuid != nullptr)
		{
			*OutGuid = Found;
		}
		return Found.IsValid() ? Cast<AActor>(Client->GetNetGuidCache()->GetObjectFromNetGUID(Found, false)) : nullptr;
	}

	UActorChannel* ObserverChannel(AActor* Actor) const
	{
		UNetDriver* Client = ObserverDriver();
		UNetConnection* Connection = Client != nullptr ? Client->ServerConnection : nullptr;
		return Connection != nullptr && Actor != nullptr ? Connection->FindActorChannelRef(Actor) : nullptr;
	}

	static FString ComponentClasses(const AActor* Actor)
	{
		TArray<FString> Names;
		if (Actor != nullptr)
		{
			for (const UActorComponent* Component : Actor->GetComponents())
			{
				if (Component != nullptr && Component->GetIsReplicated())
				{
					Names.Add(Component->GetClass()->GetName());
				}
			}
		}
		Names.Sort();
		return FString::Join(Names, TEXT(","));
	}

	/** Samples the ally machine's teammate list every frame until WindowEnd, appending each change to Shape (Ally is read when the step runs). */
	void SampleEntry(const TCHAR* Label, const TWeakObjectPtr<AStealthPlayerController>* Ally, TArray<FString>* Shape)
	{
		Network
			.Then(*FString::Printf(TEXT("%s: start"), Label), [this, Ally]()
			{
				LastMates = RemovalAudiencesPieData::MatesOf(MachineOf(*Ally));
				WindowEnd = Now() + RemovalAudiencesPieData::EntryWindowSec;
			})
			.Until(Label, [this, Ally, Shape]()
			{
				const TArray<FTeamViewTeammateEntry> Mates = RemovalAudiencesPieData::MatesOf(MachineOf(*Ally));
				const FString Diff = RemovalAudiencesPieData::ListDiff(LastMates, Mates);
				if (!Diff.IsEmpty())
				{
					Shape->Add(Diff);
				}
				LastMates = Mates;
				return Now() >= WindowEnd;
			});
	}

	TEST_METHOD(ExileReachesEachAudienceAsAnNpcKillOrAnAssassination)
	{
		using namespace StealthAssassinationPie;
		using namespace StealthPenaltyPresentationPie;
		const int32 L = Limit();
		Network
			.Until(TEXT("the round is Active, the crowd is up and the players split 2v2 (A and C on client machines)"), [this]()
			{
				const TArray<AStealthPlayerController*> All = ServerPlayers();
				if (!RoundActive() || All.Num() != 4 || StealthPenaltyPie::Penalty() == nullptr)
				{
					return false;
				}
				// A: a remote player. C: a remote opponent of A. B: A's ally. D: C's ally.
				for (AStealthPlayerController* Candidate : All)
				{
					if (Candidate->IsLocalController())
					{
						continue;
					}
					for (AStealthPlayerController* Other : All)
					{
						if (Other != Candidate && !Other->IsLocalController() && TeamOf(Other) != TeamOf(Candidate))
						{
							A = Candidate;
							C = Other;
						}
					}
					if (A.IsValid())
					{
						break;
					}
				}
				if (!A.IsValid() || !C.IsValid())
				{
					return false;
				}
				for (AStealthPlayerController* Other : All)
				{
					if (Other != A.Get() && TeamOf(Other) == TeamOf(A.Get()))
					{
						B = Other;
					}
					if (Other != C.Get() && TeamOf(Other) == TeamOf(C.Get()))
					{
						D = Other;
					}
				}
				return B.IsValid() && D.IsValid() && PawnOf(A) != nullptr && MachineOf(A) != nullptr && MachineOf(B) != nullptr && MachineOf(C) != nullptr
					&& MachineOf(D) != nullptr && RemovalAudiencesPieData::MateOf(MachineOf(B)) != nullptr && RemovalAudiencesPieData::MateOf(MachineOf(C)) != nullptr;
			})
			.Then(TEXT("place A clear of the bushes; C 10 m to its side (A's body stays relevant to C)"), [this]()
			{
				MoveClearOfBushes(PawnOf(A));
				const FVector Right = FVector::CrossProduct(FVector::UpVector, PawnOf(A)->GetActorForwardVector().GetSafeNormal2D());
				PawnOf(C)->TeleportTo(PawnOf(A)->GetActorLocation() + Right * 1000.0, PawnOf(C)->GetActorRotation());
				PawnOf(B)->TeleportTo(PawnOf(A)->GetActorLocation() - Right * 1000.0, PawnOf(B)->GetActorRotation());
			});
		Wait(TEXT("settle"), RemovalAudiencesPieData::SettleSec);

		// 1. The Assassinated reference: A kills D; C's entry of D.
		Network.Then(TEXT("A kills D (Assassinated)"), [this]()
		{
			PlaceFacing(PawnOf(D), PawnOf(A), 150.0);
			StrikeAt(PawnOf(A), PawnOf(D), Seq);
		});
		SampleEntry(TEXT("C's entry of D after the assassination"), &C, &AssassinatedShape);
		Wait(TEXT("A's lockout"), 0.2);

		// 2. A's NPC kills: kill L-1 is the NPC-kill window, kill L the exile window.
		for (int32 Kill = 0; Kill < L; ++Kill)
		{
			const bool bNpcWindow = Kill == L - 2;
			const bool bExile = Kill == L - 1;
			Network.Then(*FString::Printf(TEXT("A's NPC kill %d of %d"), Kill + 1, L), [this, bNpcWindow, bExile]()
			{
				if (bNpcWindow || bExile)
				{
					Body = PawnOf(A);
					Copy = ObserverCopy(Body.Get(), &Guid);
					Channel = ObserverChannel(Copy.Get());
					(bExile ? ExileBefore : NpcBefore) = RemovalAudiencesPieData::Snapshot(Copy.Get());
					if (bExile)
					{
						ComponentClassesBefore = ComponentClasses(Copy.Get());
					}
				}
				NpcKill(PawnOf(A), Seq);
			});
			// Every window is EntryWindowSec long (past the lockout), so the NPC-kill and exile windows compare like for like.
			if (bExile)
			{
				SampleEntry(TEXT("B's entry of A after the exile"), &B, &ExileShape);
			}
			else
			{
				Wait(TEXT("lockout"), RemovalAudiencesPieData::EntryWindowSec);
			}
			if (bNpcWindow)
			{
				Network.Then(TEXT("snapshot the NPC-kill window"), [this]() { NpcChanged = RemovalAudiencesPieData::Changed(NpcBefore, RemovalAudiencesPieData::Snapshot(Copy.Get())); });
			}
		}

		Network.Then(TEXT("assert the removal audiences"), [this]()
		{
			using namespace RemovalAudiencesPieData;
			FNetworkGUID GuidAfter;
			AActor* CopyAfter = ObserverCopy(Body.Get(), &GuidAfter);
			UActorChannel* ChannelAfter = ObserverChannel(CopyAfter);
			ExileChanged = Changed(ExileBefore, Snapshot(CopyAfter));

			// bExiled per machine.
			int32 Exiled[4] = { 0, 0, 0, 0 };
			const TWeakObjectPtr<AStealthPlayerController> Players[4] = { A, B, C, D };
			for (int32 Index = 0; Index < 4; ++Index)
			{
				if (const AStealthPlayerController* Machine = MachineOf(Players[Index]))
				{
					for (const FCommandReply& Reply : Machine->GetReceivedReplies())
					{
						Exiled[Index] += Reply.Kind == ECommandReplyKind::StrikeResult && Reply.Strike.bExiled ? 1 : 0;
					}
				}
			}
			TSet<FString> Leak;
			TSet<FString> ExileNonIdentity;
			for (const FString& Name : ExileChanged)
			{
				(IsIdentityLeakField(Name) ? Leak : ExileNonIdentity).Add(Name);
			}
			TSet<FString> NpcNonIdentity;
			for (const FString& Name : NpcChanged)
			{
				if (!IsIdentityLeakField(Name))
				{
					NpcNonIdentity.Add(Name);
				}
			}
			TestRunner->AddInfo(FString::Printf(TEXT("NKP016 bExiled replies received: A %d, B %d, C %d, D %d"), Exiled[0], Exiled[1], Exiled[2], Exiled[3]));
			TestRunner->AddInfo(FString::Printf(TEXT("NKP016 ally shape: Assassinated (C sees D) [%s]; exile (B sees A) [%s]"), *FString::Join(AssassinatedShape, TEXT(" | ")),
				*FString::Join(ExileShape, TEXT(" | "))));
			TestRunner->AddInfo(FString::Printf(TEXT("NKP016 opponent C, A's body %s guid %s ch %d -> %s guid %s ch %d; components [%s] -> [%s]"), *GetNameSafe(Copy.Get()),
				*Guid.ToString(), Channel.IsValid() ? Channel->ChIndex : -1, *GetNameSafe(CopyAfter), *GuidAfter.ToString(), ChannelAfter != nullptr ? ChannelAfter->ChIndex : -1,
				*ComponentClassesBefore, *ComponentClasses(CopyAfter)));
			TestRunner->AddInfo(FString::Printf(TEXT("NKP016 opponent C, non-movement changes: NPC-kill window [%s]; exile window [%s]; of which IVP-018 identity fields (logged, Blocked) [%s]"),
				*Join(NpcChanged), *Join(ExileChanged), *Join(Leak)));
			TestRunner->AddInfo(TEXT("NKP016 NOT MEASURED in process: the bunch cadence (ADR-0008 section 6 multi-process raw-bunch capture)"));
			for (const TWeakObjectPtr<AStealthPlayerController>& Viewer : { B, C })
			{
				FString Line;
				for (const FTeamViewTeammateEntry& Mate : MatesOf(MachineOf(Viewer)))
				{
					Line += FString::Printf(TEXT(" {handle %u state %u pawn %s retire %.3f}"), Mate.Handle, Mate.State, *GetNameSafe(Mate.PawnRef.Get()), Mate.RetireAtServerTime);
				}
				const AStealthPlayerController* Machine = MachineOf(Viewer);
				TestRunner->AddInfo(FString::Printf(TEXT("NKP016 %s's machine %s (netmode %d) mates now:%s; roster states A %d D %d"), Viewer == B ? TEXT("B") : TEXT("C"), *GetNameSafe(Machine),
					Machine != nullptr ? static_cast<int32>(Machine->GetNetMode()) : -1, *Line,
					static_cast<int32>(StealthPenaltyPie::Roster()->FindRecord(StealthPenaltyPie::ParticipantOf(A.Get()))->State),
					static_cast<int32>(StealthPenaltyPie::Roster()->FindRecord(StealthPenaltyPie::ParticipantOf(D.Get()))->State)));
			}

			ASSERT_THAT(IsTrue(Exiled[0] == 1, TEXT("positive control: the exiled player's machine received exactly one FStrikeResult with bExiled")));
			ASSERT_THAT(IsTrue(Exiled[1] == 0 && Exiled[2] == 0 && Exiled[3] == 0, TEXT("no other machine received bExiled")));
			ASSERT_THAT(IsTrue(AssassinatedShape.Num() > 0, TEXT("fixture: the Assassinated removal changed C's entry of D (the reference shape exists)")));
			ASSERT_THAT(IsTrue(ExileShape == AssassinatedShape, TEXT("the ally receives the Team-audience state change in the same shape as an Assassinated removal")));
			ASSERT_THAT(IsTrue(Copy.IsValid() && CopyAfter == Copy.Get() && GuidAfter == Guid, TEXT("the opponent keeps the same actor and NetGUID for the released pawn")));
			ASSERT_THAT(IsTrue(ChannelAfter != nullptr && ChannelAfter == Channel.Get(), TEXT("the opponent's actor channel for it neither closed nor reopened")));
			ASSERT_THAT(IsTrue(ComponentClasses(CopyAfter) == ComponentClassesBefore, TEXT("the released pawn's replicated component classes are unchanged")));
			ASSERT_THAT(IsTrue(ExileNonIdentity.Num() == NpcNonIdentity.Num() && ExileNonIdentity.Includes(NpcNonIdentity),
				TEXT("apart from movement and the IVP-018 identity fields, the opponent sees the same property changes as in an NPC kill by the same attacker")));
		});
	}
};

#endif
