// Story asn-017: the harness presentation log and the replicated-property watcher. See PresentationLogHook.h.

#include "Tests/PIE/Assassination/PresentationLogHook.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "StealthHUD.h"
#include "StealthPlayerController.h"
#include "UObject/UnrealType.h"
#include "View/Core/StealthPawn.h"
#include "View/EncounterPresentation.h"
#include "View/EncounterViewSubsystem.h"

namespace PresentationLogHookLocal
{
	/** Every PIE world, the server first, then the clients by PIE instance (StealthAssassinationPie::Worlds()). */
	TArray<UWorld*> Worlds()
	{
		TArray<const FWorldContext*> Contexts;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
			{
				Contexts.Add(&Context);
			}
		}
		Contexts.Sort([](const FWorldContext& A, const FWorldContext& B)
		{
			const bool bServerA = A.World()->GetNetMode() != NM_Client;
			const bool bServerB = B.World()->GetNetMode() != NM_Client;
			return bServerA != bServerB ? bServerA : A.PIEInstance < B.PIEInstance;
		});
		TArray<UWorld*> Out;
		for (const FWorldContext* Context : Contexts)
		{
			Out.Add(Context->World());
		}
		return Out;
	}

	UWorld* WorldOf(int32 Machine)
	{
		const TArray<UWorld*> All = Worlds();
		return All.IsValidIndex(Machine) ? All[Machine] : nullptr;
	}

	double Now(const UWorld* World)
	{
		const AGameStateBase* State = World != nullptr ? World->GetGameState() : nullptr;
		return State != nullptr ? State->GetServerWorldTimeSeconds() : 0.0;
	}

	AStealthPlayerController* LocalOf(UWorld* World)
	{
		return World != nullptr ? Cast<AStealthPlayerController>(World->GetFirstPlayerController()) : nullptr;
	}

	/** Properties whose values are the movement replication and the character movement's support data. */
	const TCHAR* const MovementProperties[] = {
		TEXT("ReplicatedMovement"), TEXT("ReplicatedBasedMovement"), TEXT("ReplicatedServerLastTransformUpdateTimeStamp"), TEXT("ReplicatedMovementMode"),
		TEXT("ReplicatedGravityDirection"), TEXT("bProxyIsJumpForceApplied"), TEXT("AnimRootMotionTranslationScale"), TEXT("RepRootMotion"),
		TEXT("RemoteViewPitch"), TEXT("bReplicateMovement"),
	};
}

namespace StealthPresentationLog
{
	void FPresentationLogHook::Begin(int32 InMachine, FPawnNamer InNamer)
	{
		using namespace PresentationLogHookLocal;
		Machine = InMachine;
		Namer = MoveTemp(InNamer);
		BeginServerTime = Now(WorldOf(Machine));
		EndServerTime = TNumericLimits<double>::Max();
		Sampled.Reset();
		LastElements.Reset();
		LastMontage.Reset();
		bLastCrouched = false;
		bStarted = false;
		Sample();
	}

	void FPresentationLogHook::End()
	{
		EndServerTime = PresentationLogHookLocal::Now(PresentationLogHookLocal::WorldOf(Machine));
	}

	void FPresentationLogHook::Sample()
	{
		using namespace PresentationLogHookLocal;
		if (EndServerTime < TNumericLimits<double>::Max())
		{
			return;
		}
		UWorld* World = WorldOf(Machine);
		AStealthPlayerController* Local = LocalOf(World);
		if (Local == nullptr)
		{
			return;
		}
		const double Time = Now(World);
		TArray<FString> Elements;
		AStealthHUD::CollectOwnerElements(*Local, Elements);
		const AStealthPawn* Own = Cast<AStealthPawn>(Local->GetPawn());
		const UAnimInstance* Anim = Own != nullptr && Own->GetMesh() != nullptr ? Own->GetMesh()->GetAnimInstance() : nullptr;
		const UAnimMontage* Montage = Anim != nullptr ? Anim->GetCurrentActiveMontage() : nullptr;
		const FString MontageName = Montage != nullptr ? Montage->GetName() : FString();
		const bool bCrouched = Own != nullptr && Own->bIsCrouched;
		if (bStarted)
		{
			for (const FString& Element : Elements)
			{
				if (!LastElements.Contains(Element))
				{
					Sampled.Add({ TEXT("Widget.+"), Element, Time });
				}
			}
			for (const FString& Element : LastElements)
			{
				if (!Elements.Contains(Element))
				{
					Sampled.Add({ TEXT("Widget.-"), Element, Time });
				}
			}
			if (MontageName != LastMontage)
			{
				Sampled.Add({ TEXT("OwnPawn.Montage"), MontageName, Time });
			}
			if (bCrouched != bLastCrouched)
			{
				Sampled.Add({ TEXT("OwnPawn.Crouch"), bCrouched ? TEXT("1") : TEXT("0"), Time });
			}
		}
		bStarted = true;
		LastElements = MoveTemp(Elements);
		LastMontage = MontageName;
		bLastCrouched = bCrouched;
	}

	TArray<FEntry> FPresentationLogHook::Collect() const
	{
		using namespace PresentationLogHookLocal;
		TArray<FEntry> Out = Sampled;
		UWorld* World = WorldOf(Machine);
		if (const UEncounterPresentationSubsystem* Presentation = UEncounterPresentationSubsystem::Get(World))
		{
			for (const FEncounterPresentationLogRow& Row : Presentation->GetLog())
			{
				const FString Name = Row.ServerNow + 1e-6 >= BeginServerTime && Namer ? Namer(Row.PawnKey) : FString();
				if (!Name.IsEmpty())
				{
					Out.Add({ FString::Printf(TEXT("Presentation.%s"), LexEncounterPresentationEvent(Row.Event)), FString::Printf(TEXT("%s:%s"), *Name, LexEncounterClip(Row.Clip)), Row.ServerNow });
				}
			}
		}
		if (const AStealthPlayerController* Local = LocalOf(World))
		{
			for (const FOwnPresentationRecord& Record : Local->GetOwnPresentationLog())
			{
				if (Record.ServerNow + 1e-6 >= BeginServerTime)
				{
					Out.Add({ FString::Printf(TEXT("Hud.%s"), LexOwnPresentationEvent(Record.Event)), FString::Printf(TEXT("%s:%d"), *Record.Text, Record.Value), Record.ServerNow });
				}
			}
			const ULocalPlayer* Player = Local->GetLocalPlayer();
			if (const UEncounterViewSubsystem* View = Player != nullptr ? Player->GetSubsystem<UEncounterViewSubsystem>() : nullptr)
			{
				for (const FTransientGradeCall& Call : View->GetGradeCalls())
				{
					if (Call.ServerNow + 1e-6 >= BeginServerTime)
					{
						Out.Add({ TEXT("Camera.Grade"), FString::FromInt(static_cast<int32>(Call.Cause)), Call.ServerNow });
					}
				}
				for (const FRoleChangeRecord& Record : View->GetRoleChanges())
				{
					if (Record.ServerNow + 1e-6 >= BeginServerTime)
					{
						Out.Add({ TEXT("Camera.Role"), LexRoleChangeReason(Record.Reason), Record.ServerNow });
					}
				}
			}
		}
		const double Until = EndServerTime;
		Out.RemoveAll([Until](const FEntry& Entry) { return Entry.ServerNow > Until; });
		Out.StableSort([](const FEntry& A, const FEntry& B) { return A.ServerNow < B.ServerNow; });
		return Out;
	}

	TArray<FEntry> Unmatched(const TArray<FEntry>& Struck, double StruckOrigin, const TArray<FEntry>& Control, double ControlOrigin, double From, double To, double Tolerance)
	{
		TArray<FEntry> Out;
		TArray<bool> Used;
		Used.Init(false, Control.Num());
		for (const FEntry& Entry : Struck)
		{
			const double Rel = Entry.ServerNow - StruckOrigin;
			if (Rel < From || Rel >= To)
			{
				continue;
			}
			bool bMatched = false;
			for (int32 Index = 0; Index < Control.Num() && !bMatched; ++Index)
			{
				if (!Used[Index] && Control[Index].Source == Entry.Source && Control[Index].Key == Entry.Key
					&& FMath::Abs((Control[Index].ServerNow - ControlOrigin) - Rel) <= Tolerance)
				{
					Used[Index] = true;
					bMatched = true;
				}
			}
			if (!bMatched)
			{
				Out.Add(Entry);
			}
		}
		return Out;
	}

	FString Describe(const TArray<FEntry>& Entries, double Origin)
	{
		TArray<FString> Lines;
		for (const FEntry& Entry : Entries)
		{
			Lines.Add(FString::Printf(TEXT("%+.3f %s %s"), Entry.ServerNow - Origin, *Entry.Source, *Entry.Key));
		}
		return Lines.IsEmpty() ? FString(TEXT("(none)")) : FString::Join(Lines, TEXT("; "));
	}

	void FReplicatedPropertyWatcher::Begin(AActor* Copy, int32 InMachine)
	{
		Actor = Copy;
		Machine = InMachine;
		Changes.Reset();
		Last.Reset();
		Snapshot(Last);
	}

	void FReplicatedPropertyWatcher::Snapshot(TMap<FString, FString>& Out) const
	{
		const AActor* Copy = Actor.Get();
		if (Copy == nullptr)
		{
			return;
		}
		auto Add = [&Out](const UObject* Object, const FString& Prefix)
		{
			for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
			{
				if (!It->HasAnyPropertyFlags(CPF_Net))
				{
					continue;
				}
				FString Value;
				It->ExportTextItem_InContainer(Value, Object, nullptr, nullptr, PPF_None);
				Out.Add(Prefix + It->GetName(), MoveTemp(Value));
			}
		};
		Add(Copy, Copy->GetClass()->GetName() + TEXT("."));
		TInlineComponentArray<UActorComponent*> Components(Copy);
		for (const UActorComponent* Component : Components)
		{
			if (Component != nullptr && Component->GetIsReplicated())
			{
				Add(Component, Component->GetClass()->GetName() + TEXT("."));
			}
		}
	}

	void FReplicatedPropertyWatcher::Sample()
	{
		using namespace PresentationLogHookLocal;
		if (!Actor.IsValid())
		{
			return;
		}
		TMap<FString, FString> Now_;
		Snapshot(Now_);
		const double Time = Now(WorldOf(Machine));
		for (const TPair<FString, FString>& Pair : Now_)
		{
			const FString* Before = Last.Find(Pair.Key);
			if (Before == nullptr || *Before != Pair.Value)
			{
				Changes.Add({ Pair.Key, Time });
			}
		}
		Last = MoveTemp(Now_);
	}

	bool FReplicatedPropertyWatcher::IsMovementProperty(const FString& Property)
	{
		FString Name = Property;
		int32 Dot = INDEX_NONE;
		if (Property.FindLastChar(TEXT('.'), Dot))
		{
			Name = Property.Mid(Dot + 1);
		}
		for (const TCHAR* Movement : PresentationLogHookLocal::MovementProperties)
		{
			if (Name == Movement)
			{
				return true;
			}
		}
		return false;
	}
}

#endif
