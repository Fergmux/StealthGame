// Stories asn-018 / asn-020 / asn-021: the one idempotent encounter presentation handler. See View/EncounterPresentation.h.

#include "View/EncounterPresentation.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BodyRemovalEvaluator.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier_SkewWarp.h"
#include "StealthGameplayModule.h"
#include "StrikeVariantCore.h"
#include "View/Core/StealthPawn.h"
#include "View/EncounterPresentationSettings.h"
#include "View/StealthTuningPublicSubstructs.h"

namespace EncounterPresentationLocal
{
	constexpr int32 MaxLogRows = 4096;

	UAnimMontage* MontageFor(EEncounterClip Clip)
	{
		const UEncounterPresentationSettings* Settings = UEncounterPresentationSettings::Get();
		const TSoftObjectPtr<UAnimMontage>* Soft = nullptr;
		switch (Clip)
		{
		case EEncounterClip::Strike_F: Soft = &Settings->StrikeFMontage; break;
		case EEncounterClip::Strike_L60: Soft = &Settings->StrikeL60Montage; break;
		case EEncounterClip::Strike_R60: Soft = &Settings->StrikeR60Montage; break;
		case EEncounterClip::Counter: Soft = &Settings->CounterMontage; break;
		case EEncounterClip::DeathFall: Soft = &Settings->DeathFallMontage; break;
		default: return nullptr;
		}
		return Soft->IsNull() ? nullptr : Soft->LoadSynchronous();
	}

	UAnimInstance* AnimOf(const AStealthPawn& Pawn)
	{
		const USkeletalMeshComponent* Mesh = Pawn.GetMesh();
		return Mesh != nullptr && Mesh->GetSkeletalMeshAsset() != nullptr ? Mesh->GetAnimInstance() : nullptr;
	}

	/** The engine half over the pawn's mesh (montages in DefaultSlot), its warping component and its capsule. */
	class FEncounterMontagePlayer final : public IEncounterClipPlayer
	{
	public:
		virtual bool Play(AStealthPawn& Pawn, const FEncounterPresentationPlan& Plan) override
		{
			UAnimInstance* Anim = AnimOf(Pawn);
			UAnimMontage* Montage = MontageFor(Plan.Clip);
			if (Anim == nullptr || Montage == nullptr)
			{
				return false;
			}
			const float Position = static_cast<float>(FMath::Clamp(Plan.Position, 0.0, static_cast<double>(Montage->GetPlayLength())));
			const float Played = Anim->Montage_PlayWithBlendIn(Montage, FAlphaBlendArgs(static_cast<float>(Plan.BlendInSec)), static_cast<float>(Plan.PlayRate),
				EMontagePlayReturnType::MontageLength, Position, /*bStopAllMontages*/ true);
			return Played > 0.0f;
		}

		virtual double GetPosition(const AStealthPawn& Pawn, EEncounterClip Clip) const override
		{
			UAnimInstance* Anim = AnimOf(Pawn);
			UAnimMontage* Montage = MontageFor(Clip);
			if (Anim == nullptr || Montage == nullptr || !Anim->Montage_IsPlaying(Montage))
			{
				return -1.0;
			}
			return Anim->Montage_GetPosition(Montage);
		}

		virtual void Seek(AStealthPawn& Pawn, EEncounterClip Clip, double Position, double PlayRate) override
		{
			UAnimInstance* Anim = AnimOf(Pawn);
			UAnimMontage* Montage = MontageFor(Clip);
			if (Anim != nullptr && Montage != nullptr && Anim->Montage_IsPlaying(Montage))
			{
				Anim->Montage_SetPosition(Montage, static_cast<float>(FMath::Clamp(Position, 0.0, static_cast<double>(Montage->GetPlayLength()))));
				Anim->Montage_SetPlayRate(Montage, static_cast<float>(PlayRate));
			}
		}

		virtual void BeginStrikeWarp(AStealthPawn& Pawn, const FEncounterPresentationPlan& Plan, const FEncounterPresentationParams& Params) override
		{
			UMotionWarpingComponent* Warping = Pawn.GetWarpingForStrike();
			UAnimMontage* Montage = MontageFor(Plan.Clip);
			if (Warping == nullptr || Montage == nullptr)
			{
				return;
			}
			// The target the pawn set once from its capsule + WarpPoint (story asn-014); faced to the bearing so the root
			// arrives the contact separation short of it, facing it. Never re-tracked.
			const FVector Offset = Pawn.GetActionState().WarpPoint.ToOffsetCm();
			const FMotionWarpingTarget* Existing = Warping->FindWarpTarget(AStealthPawn::StrikeWarpTargetName);
			if (Existing == nullptr)
			{
				return; // the pawn sets the target once per write (asn-014); without it there is nothing to warp to
			}
			const FVector Target = Existing->GetLocation();
			const double FacingYaw = Pawn.GetActorRotation().Yaw + FStrikeVariantCore::BearingOfOffset(Offset, Pawn.GetActorRotation().Yaw);
			Warping->AddOrUpdateWarpTargetFromLocationAndRotation(AStealthPawn::StrikeWarpTargetName, Target, FRotator(0.0, FacingYaw, 0.0));

			const UCapsuleComponent* Capsule = Pawn.GetCapsuleComponent();
			const double Separation = 2.0 * (Capsule != nullptr ? Capsule->GetScaledCapsuleRadius() : 30.0) + Params.StrikeContactGapCm;
			const float WindowEnd = static_cast<float>(FMath::Min(Plan.WarpWindowEndSec, static_cast<double>(Montage->GetPlayLength())));
			// The clip's warp point: the victim's capsule centre, Separation ahead of the root at the contact frame (mesh
			// space: the mannequin faces +Y; the warping utilities rotate it by the mesh's base rotation). Translation only:
			// the D54 placeholder is one forward clip with no authored entry turn, so warping the rotation to the bearing would
			// turn the body by up to 90 degrees, past StrikeWarpYawLimitDeg (30). Rotation warp is off until the authored
			// Strike_L60 / Strike_R60 clips land (their entry turn plus <= 30 degrees of warp); the body keeps its facing.
			const FTransform RootAtContact = UMotionWarpingUtilities::ExtractRootTransformFromAnimation(Montage, WindowEnd);
			const FVector MeshForward = Pawn.GetBaseRotationOffset().Inverse().RotateVector(FVector::XAxisVector);
			const FTransform WarpPoint(FQuat::Identity, RootAtContact.GetLocation() + MeshForward * Separation);
			URootMotionModifier_SkewWarp::AddRootMotionModifierSkewWarp(Warping, Montage, 0.0f, WindowEnd, AStealthPawn::StrikeWarpTargetName,
				EWarpPointAnimProvider::Static, WarpPoint, NAME_None, /*bWarpTranslation*/ true, /*bIgnoreZ*/ true, /*bWarpRotation*/ false);
		}

		virtual void SetPawnCollisionOff(AStealthPawn& Pawn) override
		{
			if (UCapsuleComponent* Capsule = Pawn.GetCapsuleComponent())
			{
				Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			}
		}

		virtual void HideBody(AStealthPawn& Pawn) override
		{
			// Client-local and in one frame: the mesh and everything attached to it (never a replicated actor flag).
			if (USkeletalMeshComponent* Mesh = Pawn.GetMesh())
			{
				Mesh->SetHiddenInGame(true, /*bPropagateToChildren*/ true);
			}
		}
	};

	/** The ViewBlock trace channel (Camera story 007's project-owned channel), else Visibility. */
	ECollisionChannel ViewBlockChannel()
	{
		static ECollisionChannel Cached = []()
		{
			const UCollisionProfile* Profile = UCollisionProfile::Get();
			for (int32 Channel = ECC_GameTraceChannel1; Channel <= ECC_GameTraceChannel18; ++Channel)
			{
				if (Profile->ReturnChannelNameFromContainerIndex(Channel) == FName(TEXT("ViewBlock")))
				{
					return static_cast<ECollisionChannel>(Channel);
				}
			}
			return ECC_Visibility;
		}();
		return Cached;
	}

	/** VA-ASN-2's geometry test from the local player's camera (pawns ignore ViewBlock, so they never occlude). */
	class FEncounterBodyViewTraceQuery final : public IEncounterBodyViewQuery
	{
	public:
		static const APlayerController* Viewer(const UWorld& World)
		{
			for (FConstPlayerControllerIterator It = World.GetPlayerControllerIterator(); It; ++It)
			{
				const APlayerController* Controller = It->Get();
				if (Controller != nullptr && Controller->IsLocalController() && Controller->PlayerCameraManager != nullptr)
				{
					return Controller;
				}
			}
			return nullptr;
		}

		virtual bool HasViewer(const UWorld& World) const override
		{
			return Viewer(World) != nullptr;
		}

		virtual void Query(const AStealthPawn& Body, bool& bOutInFrustum, bool& bOutFullyOccluded) const override
		{
			bOutInFrustum = false;
			bOutFullyOccluded = false;
			const UWorld* World = Body.GetWorld();
			const APlayerController* Controller = World != nullptr ? Viewer(*World) : nullptr;
			const USkeletalMeshComponent* Mesh = Body.GetMesh();
			if (Controller == nullptr || Mesh == nullptr)
			{
				return;
			}
			const FMinimalViewInfo View = Controller->PlayerCameraManager->GetCameraCacheView();
			const double HalfH = FMath::DegreesToRadians(FMath::Clamp(static_cast<double>(View.FOV), 1.0, 170.0) * 0.5);
			const double Aspect = View.AspectRatio > 0.1f ? View.AspectRatio : 16.0 / 9.0;
			const double TanH = FMath::Tan(HalfH);
			const double TanV = TanH / Aspect;
			const FBox Box = Mesh->Bounds.GetBox().ExpandBy(-2.0);
			const FVector Center = Box.GetCenter();
			const FVector Extent = Box.GetExtent();
			FCollisionQueryParams Params(SCENE_QUERY_STAT(EncounterBodyRemoval), /*bTraceComplex*/ false);
			Params.AddIgnoredActor(&Body);
			int32 InFrustum = 0;
			int32 Blocked = 0;
			for (int32 Sample = 0; Sample < 9; ++Sample)
			{
				const FVector Point = Sample == 8 ? Center : Center + Extent * FVector((Sample & 1) ? 1.0 : -1.0, (Sample & 2) ? 1.0 : -1.0, (Sample & 4) ? 1.0 : -1.0);
				const FVector Local = View.Rotation.UnrotateVector(Point - View.Location);
				if (Local.X <= 1.0 || FMath::Abs(Local.Y) > Local.X * TanH || FMath::Abs(Local.Z) > Local.X * TanV)
				{
					continue;
				}
				++InFrustum;
				FHitResult Hit;
				if (World->LineTraceSingleByChannel(Hit, View.Location, Point, ViewBlockChannel(), Params))
				{
					++Blocked;
				}
			}
			bOutInFrustum = InFrustum > 0;
			bOutFullyOccluded = InFrustum > 0 && Blocked == InFrustum;
		}
	};

	/** The owner side over UEncounterViewSubsystem (the locally possessed pawn's local player). */
	class FEncounterViewOwnerPresentation final : public IEncounterOwnerPresentation
	{
	public:
		virtual bool IsLocallyViewed(const AStealthPawn& Pawn) const override
		{
			return UEncounterViewSubsystem::FindForPawn(&Pawn) != nullptr;
		}

		virtual bool WasPossessedAsInformant(const AStealthPawn& Pawn) const override
		{
			const UEncounterViewSubsystem* View = UEncounterViewSubsystem::FindForPawn(&Pawn);
			return View != nullptr && View->WasPossessedAsInformant(&Pawn);
		}

		virtual void DispatchRoleChange(const AStealthPawn& Pawn, ERoleChangeReason Reason, double ServerNow, double DeathServerTime) override
		{
			if (UEncounterViewSubsystem* View = UEncounterViewSubsystem::FindForPawn(&Pawn))
			{
				View->DispatchRoleChange(&Pawn, Reason, ServerNow, DeathServerTime);
			}
		}

		virtual void ApplyTransientGrade(const AStealthPawn& Pawn, ETransientGradeCause Cause, double ServerNow, double DueServerTime) override
		{
			if (UEncounterViewSubsystem* View = UEncounterViewSubsystem::FindForPawn(&Pawn))
			{
				View->ApplyTransientGrade(Cause, ServerNow, DueServerTime);
			}
		}
	};
}

const TCHAR* LexEncounterPresentationEvent(EEncounterPresentationEvent Event)
{
	switch (Event)
	{
	case EEncounterPresentationEvent::ClipStart: return TEXT("ClipStart");
	case EEncounterPresentationEvent::ClipSeek: return TEXT("ClipSeek");
	case EEncounterPresentationEvent::WarpBegin: return TEXT("WarpBegin");
	case EEncounterPresentationEvent::CollisionOff: return TEXT("CollisionOff");
	case EEncounterPresentationEvent::BodyHidden: return TEXT("BodyHidden");
	case EEncounterPresentationEvent::RoleChange: return TEXT("RoleChange");
	case EEncounterPresentationEvent::Grade: return TEXT("Grade");
	default: return TEXT("UnsyncedFallback");
	}
}

UEncounterPresentationSubsystem::UEncounterPresentationSubsystem()
{
	Params = FEncounterPresentationParams::FromPublic(FAssassinationTuning_Public());
}

UEncounterPresentationSubsystem* UEncounterPresentationSubsystem::Get(const UWorld* World)
{
	return World != nullptr ? World->GetSubsystem<UEncounterPresentationSubsystem>() : nullptr;
}

double UEncounterPresentationSubsystem::EstimatedServerNow(const UWorld* World)
{
	const AGameStateBase* State = World != nullptr ? World->GetGameState() : nullptr;
	return State != nullptr ? State->GetServerWorldTimeSeconds() : (World != nullptr ? World->GetTimeSeconds() : 0.0);
}

bool UEncounterPresentationSubsystem::IsServerClockSynced(const UWorld* World)
{
	if (World == nullptr || World->GetNetMode() != NM_Client)
	{
		return true;
	}
	return World->GetGameState() != nullptr && World->GetRealTimeSeconds() >= UEncounterPresentationSettings::Get()->UnsyncedClockGraceSec;
}

bool UEncounterPresentationSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UEncounterPresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const UEncounterPresentationSettings* Settings = UEncounterPresentationSettings::Get();
	Timings.StrikeClipContactSec = Settings->StrikeClipContactSec;
	// The clip lengths are the montages' own (the timeline maps them onto the tuned timings).
	if (const UAnimMontage* Strike = EncounterPresentationLocal::MontageFor(EEncounterClip::Strike_F))
	{
		Timings.StrikeLengthSec = Strike->GetPlayLength();
	}
	if (const UAnimMontage* Counter = EncounterPresentationLocal::MontageFor(EEncounterClip::Counter))
	{
		Timings.CounterLengthSec = Counter->GetPlayLength();
	}
	if (const UAnimMontage* Fall = EncounterPresentationLocal::MontageFor(EEncounterClip::DeathFall))
	{
		Timings.FallLengthSec = Fall->GetPlayLength();
	}
	ProductionPlayer = MakeUnique<EncounterPresentationLocal::FEncounterMontagePlayer>();
	ProductionViewQuery = MakeUnique<EncounterPresentationLocal::FEncounterBodyViewTraceQuery>();
	ProductionOwner = MakeUnique<EncounterPresentationLocal::FEncounterViewOwnerPresentation>();
	StateChangedHandle = AStealthPawn::OnAnyEncounterStateChanged().AddUObject(this, &UEncounterPresentationSubsystem::HandleEncounterStateChanged);
}

void UEncounterPresentationSubsystem::Deinitialize()
{
	AStealthPawn::OnAnyEncounterStateChanged().Remove(StateChangedHandle);
	States.Reset();
	Scheduled.Reset();
	Super::Deinitialize();
}

TStatId UEncounterPresentationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEncounterPresentationSubsystem, STATGROUP_Tickables);
}

void UEncounterPresentationSubsystem::SetClipPlayer(IEncounterClipPlayer* InPlayer) { PlayerOverride = InPlayer; }
void UEncounterPresentationSubsystem::SetBodyViewQuery(IEncounterBodyViewQuery* Query) { ViewQueryOverride = Query; }
void UEncounterPresentationSubsystem::SetOwnerPresentation(IEncounterOwnerPresentation* InOwner) { OwnerOverride = InOwner; }

void UEncounterPresentationSubsystem::SetClockOverride(TFunction<double()> InNow, TFunction<bool()> Synced)
{
	NowOverride = MoveTemp(InNow);
	SyncedOverride = MoveTemp(Synced);
}

IEncounterClipPlayer& UEncounterPresentationSubsystem::Player() const { return PlayerOverride != nullptr ? *PlayerOverride : *ProductionPlayer; }
IEncounterBodyViewQuery& UEncounterPresentationSubsystem::ViewQuery() const { return ViewQueryOverride != nullptr ? *ViewQueryOverride : *ProductionViewQuery; }
IEncounterOwnerPresentation& UEncounterPresentationSubsystem::Owner() const { return OwnerOverride != nullptr ? *OwnerOverride : *ProductionOwner; }

double UEncounterPresentationSubsystem::Now() const
{
	return NowOverride ? NowOverride() : EstimatedServerNow(GetWorld());
}

bool UEncounterPresentationSubsystem::ClockSynced() const
{
	return SyncedOverride ? SyncedOverride() : IsServerClockSynced(GetWorld());
}

void UEncounterPresentationSubsystem::HandleEncounterStateChanged(AStealthPawn* Pawn)
{
	if (Pawn != nullptr && Pawn->GetWorld() == GetWorld())
	{
		// Bodies are destroyed (removal, Reset) without a last event: forget gone pawns once the table grows.
		if (States.Num() > 256)
		{
			for (auto It = States.CreateIterator(); It; ++It)
			{
				if (!It.Key().IsValid())
				{
					Scheduled.Remove(It.Key());
					It.RemoveCurrent();
				}
			}
		}
		Refresh(*Pawn, Now());
	}
}

void UEncounterPresentationSubsystem::Tick(float DeltaTime)
{
	if (Scheduled.IsEmpty())
	{
		return;
	}
	const double TimeNow = Now();
	TArray<TWeakObjectPtr<AStealthPawn>> Due = Scheduled.Array();
	for (const TWeakObjectPtr<AStealthPawn>& Weak : Due)
	{
		if (AStealthPawn* Pawn = Weak.Get())
		{
			Refresh(*Pawn, TimeNow);
		}
		else
		{
			Scheduled.Remove(Weak);
			States.Remove(Weak);
		}
	}
}

void UEncounterPresentationSubsystem::AddRow(EEncounterPresentationEvent Event, const AStealthPawn& Pawn, EEncounterClip Clip, double TimeNow, double Value, double Due, bool bLate)
{
	if (Log.Num() >= EncounterPresentationLocal::MaxLogRows)
	{
		Log.RemoveAt(0, Log.Num() / 2);
	}
	FEncounterPresentationLogRow& Row = Log.AddDefaulted_GetRef();
	Row.Event = Event;
	Row.PawnKey = &Pawn;
	Row.PawnName = Pawn.GetName();
	Row.Clip = Clip;
	Row.ServerNow = TimeNow;
	Row.Value = Value;
	Row.DueServerTime = Due;
	Row.bLate = bLate;
	UE_LOGF(LogStealthGameplay, Verbose, "[EncounterPresentation] %ls %ls %ls now %.3f value %.3f due %.3f late %d (net mode %d)", LexEncounterPresentationEvent(Event),
		*Row.PawnName, LexEncounterClip(Clip), TimeNow, Value, Due, bLate ? 1 : 0, GetWorld() != nullptr ? static_cast<int32>(GetWorld()->GetNetMode()) : -1);
}

TArray<FEncounterPresentationLogRow> UEncounterPresentationSubsystem::FindRows(const AStealthPawn* Pawn, EEncounterPresentationEvent Event) const
{
	TArray<FEncounterPresentationLogRow> Out;
	for (const FEncounterPresentationLogRow& Row : Log)
	{
		if (Row.PawnKey == Pawn && Row.Event == Event)
		{
			Out.Add(Row);
		}
	}
	return Out;
}

const FEncounterPresentationPlan* UEncounterPresentationSubsystem::FindLastPlan(const AStealthPawn* Pawn) const
{
	const FPawnState* State = States.Find(TWeakObjectPtr<AStealthPawn>(const_cast<AStealthPawn*>(Pawn)));
	return State != nullptr ? &State->LastPlan : nullptr;
}

bool UEncounterPresentationSubsystem::IsScheduled(const AStealthPawn* Pawn) const
{
	return Scheduled.Contains(TWeakObjectPtr<AStealthPawn>(const_cast<AStealthPawn*>(Pawn)));
}

void UEncounterPresentationSubsystem::Refresh(AStealthPawn& Pawn, double TimeNow)
{
	using namespace EncounterPresentationLocal;
	const TWeakObjectPtr<AStealthPawn> Key(&Pawn);
	FPawnState& State = States.FindOrAdd(Key);
	const FActionState& Action = Pawn.GetActionState();
	const FDeathState& Death = Pawn.GetDeathState();

	// Receipt times: what this machine first saw, never what it should have seen.
	if (Action.Seq != State.SeenSeq)
	{
		State.SeenSeq = Action.Seq;
		State.ActionReceiptNow = TimeNow;
	}
	if (Death.DeathServerTime >= 0.0 && !State.bDeathSeen)
	{
		State.bDeathSeen = true;
		State.DeathReceiptNow = TimeNow;
	}

	FEncounterPresentationInput Input;
	Input.Action = Action;
	Input.Death = Death;
	Input.Now = TimeNow;
	Input.bClockSynced = ClockSynced();
	Input.ActionReceiptNow = State.ActionReceiptNow;
	Input.DeathReceiptNow = State.DeathReceiptNow;
	const FEncounterPresentationPlan Plan = FEncounterPresentationCore::Evaluate(Params, Timings, Input);
	State.LastPlan = Plan;

	if (Plan.bUnsyncedFallback && !State.bLoggedUnsynced)
	{
		State.bLoggedUnsynced = true;
		AddRow(EEncounterPresentationEvent::UnsyncedFallback, Pawn, Plan.Clip, TimeNow, Plan.Position, -1.0);
		UE_LOGF(LogStealthGameplay, Log, "[EncounterPresentation] %ls: server clock not synced, presenting on receipt", *Pawn.GetName());
	}

	// 1 The clip (seeked, never restarted; a strike that ended before this machine saw it is not started late).
	if (Plan.Clip != EEncounterClip::None && Plan.Position >= 0.0 && !Plan.bNeverShow && !State.bHidden)
	{
		const bool bNewInstance = State.PlayingClip != Plan.Clip || State.PlayingKey != Plan.InstanceKey;
		if (bNewInstance)
		{
			State.PlayingClip = Plan.Clip;
			State.PlayingKey = Plan.InstanceKey;
			State.PlayingRate = Plan.PlayRate;
			if (!Plan.bClipEnded || Plan.Clip == EEncounterClip::DeathFall)
			{
				const bool bPlayed = Player().Play(Pawn, Plan);
				AddRow(EEncounterPresentationEvent::ClipStart, Pawn, Plan.Clip, TimeNow, Plan.Position,
					Plan.Clip == EEncounterClip::DeathFall ? Death.DeathServerTime : Action.ContactServerTime, Plan.bLate);
				if (bPlayed && IsStrikeClip(Plan.Clip) && Plan.Position < Plan.WarpWindowEndSec)
				{
					Player().BeginStrikeWarp(Pawn, Plan, Params);
					AddRow(EEncounterPresentationEvent::WarpBegin, Pawn, Plan.Clip, TimeNow, Plan.WarpWindowEndSec, Action.ContactServerTime);
				}
			}
		}
		else if (!Plan.bClipEnded)
		{
			const double Playing = Player().GetPosition(Pawn, Plan.Clip);
			const double Tolerance = UEncounterPresentationSettings::Get()->SeekToleranceSec * FMath::Max(1.0, Plan.PlayRate);
			const bool bRateChanged = !FMath::IsNearlyEqual(State.PlayingRate, Plan.PlayRate, 1e-3);
			if (Playing >= 0.0 && (bRateChanged || FMath::Abs(Playing - Plan.Position) > Tolerance))
			{
				Player().Seek(Pawn, Plan.Clip, Plan.Position, Plan.PlayRate);
				State.PlayingRate = Plan.PlayRate;
				if (FMath::Abs(Playing - Plan.Position) > Tolerance)
				{
					AddRow(EEncounterPresentationEvent::ClipSeek, Pawn, Plan.Clip, TimeNow, Plan.Position, Playing);
				}
			}
		}
	}

	// 2 Collision off pawn channels, client-locally, at DeathServerTime + DeathFallSec.
	if (Plan.bCollisionOff && !State.bCollisionOff)
	{
		State.bCollisionOff = true;
		Player().SetPawnCollisionOff(Pawn);
		AddRow(EEncounterPresentationEvent::CollisionOff, Pawn, Plan.Clip, TimeNow, 0.0, Death.DeathServerTime + Params.DeathFallSec);
	}

	// 3 The removal (VA-ASN-2): geometry and time only.
	if (Plan.bDeathPresented && !State.bHidden && GetWorld() != nullptr && ViewQuery().HasViewer(*GetWorld())
		&& (Plan.bNeverShow || Plan.BodyPhase == EEncounterBodyPhase::Removal || Plan.BodyPhase == EEncounterBodyPhase::Cut))
	{
		FBodyRemovalInput Removal;
		Removal.Now = TimeNow;
		Removal.DeathServerTime = Death.DeathServerTime;
		Removal.DeathFallSec = Params.DeathFallSec;
		Removal.BodyLingerSec = Params.BodyLingerSec;
		Removal.BodyRemovalGraceSec = Params.BodyRemovalGraceSec;
		Removal.bFirstSeenAfterLinger = Plan.bNeverShow;
		ViewQuery().Query(Pawn, Removal.bInFrustum, Removal.bFullyOccluded);
		const EBodyRemovalDecision Decision = FBodyRemovalEvaluator::Evaluate(Removal);
		if (FBodyRemovalEvaluator::Hides(Decision))
		{
			State.bHidden = true;
			Player().HideBody(Pawn);
			AddRow(EEncounterPresentationEvent::BodyHidden, Pawn, Plan.Clip, TimeNow, static_cast<double>(Decision), FBodyRemovalEvaluator::WindowOpensAt(Removal));
			UE_LOGF(LogStealthGameplay, Log, "[EncounterPresentation] %ls hidden (%ls) at %.3f, DeathServerTime %.3f", *Pawn.GetName(), LexBodyRemovalDecision(Decision), TimeNow, Death.DeathServerTime);
		}
	}

	// 4 The owner: the role change once per FDeathState at its DeathServerTime, with the victim's grade; the bound
	// defender's grade at its Counter's contact.
	if (Plan.bDeathPresented && !State.bRoleDispatched)
	{
		State.bRoleDispatched = true;
		if (Owner().IsLocallyViewed(Pawn))
		{
			const bool bReversed = Action.Pose == EActionPose::Strike && FMath::Abs(Action.ContactServerTime - Death.DeathServerTime) < 0.001;
			const ERoleChangeReason Reason = bReversed ? ERoleChangeReason::ReversedAgainst
				: (Owner().WasPossessedAsInformant(Pawn) ? ERoleChangeReason::InformantKilled : ERoleChangeReason::Assassinated);
			Owner().DispatchRoleChange(Pawn, Reason, TimeNow, Death.DeathServerTime);
			AddRow(EEncounterPresentationEvent::RoleChange, Pawn, Plan.Clip, TimeNow, static_cast<double>(Reason), Death.DeathServerTime);
			Owner().ApplyTransientGrade(Pawn, ETransientGradeCause::VictimDeath, TimeNow, Death.DeathServerTime);
			AddRow(EEncounterPresentationEvent::Grade, Pawn, Plan.Clip, TimeNow, static_cast<double>(ETransientGradeCause::VictimDeath), Death.DeathServerTime);
		}
	}
	if (Action.Pose == EActionPose::Counter && Action.ContactServerTime >= 0.0 && TimeNow + 0.001 >= Action.ContactServerTime
		&& State.CounterGradedSeq != Action.Seq && Death.DeathServerTime < 0.0)
	{
		State.CounterGradedSeq = Action.Seq;
		if (Owner().IsLocallyViewed(Pawn))
		{
			Owner().ApplyTransientGrade(Pawn, ETransientGradeCause::DefenderCounter, TimeNow, Action.ContactServerTime);
			AddRow(EEncounterPresentationEvent::Grade, Pawn, Plan.Clip, TimeNow, static_cast<double>(ETransientGradeCause::DefenderCounter), Action.ContactServerTime);
		}
	}

	// 5 The schedule: tick only while the plan needs it (a pending Counter's grade too).
	const bool bCounterPending = Action.Pose == EActionPose::Counter && State.CounterGradedSeq != Action.Seq;
	if ((Plan.bNeedsTick || bCounterPending) && !(State.bHidden && Plan.bDeathPresented))
	{
		Scheduled.Add(Key);
	}
	else
	{
		Scheduled.Remove(Key);
	}
}
