#include "Server/Core/StealthPawnCommandComponent.h"

#include "GameFramework/Controller.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Server/ActionHub.h"
#include "View/Core/StealthPawn.h"
#include "View/Core/StealthPawnMovementComponent.h"

const FName UPawnCommandComponent::ComponentName(TEXT("PawnCommand"));

UPawnCommandComponent::UPawnCommandComponent()
{
	SetIsReplicatedByDefault(false);
	PrimaryComponentTick.bCanEverTick = true;
	// Ticks only while a Move intent is held (Move enables it), so idle bodies cost nothing.
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// Before the movement component consumes the input vector (BeginPlay adds the explicit prerequisite).
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UPawnCommandComponent::BeginPlay()
{
	Super::BeginPlay();

	APawn* Pawn = GetOwner<APawn>();
	if (UPawnMovementComponent* Movement = Pawn ? Pawn->GetMovementComponent() : nullptr)
	{
		Movement->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick);
	}

	// Server: the composition root's wiring (queue, clock, seams) lives on the world's action hub.
	if (const UActionHub* Hub = UActionHub::Get(GetWorld()))
	{
		if (const FPawnCommandCoreDeps* Deps = Hub->GetCommandWiring())
		{
			Core.Initialize(*Deps);
		}
	}
}

void UPawnCommandComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// The held intent is re-applied every tick for every source (ADR-0011 section 1, Crowd CNB-R3.2).
	if (!MoveIntent.IsNearlyZero())
	{
		if (APawn* Pawn = GetOwner<APawn>())
		{
			Pawn->AddMovementInput(GetWorldMoveInput());
		}
	}
}

FVector UPawnCommandComponent::GetWorldMoveInput() const
{
	const APawn* Pawn = GetOwner<APawn>();
	if (!Pawn)
	{
		return FVector::ZeroVector;
	}
	const FRotator Yaw(0.0, Pawn->GetControlRotation().Yaw, 0.0);
	const FRotationMatrix Basis(Yaw);
	const FVector2D Clamped = MoveIntent.GetSafeNormal() * FMath::Min(MoveIntent.Size(), 1.0);
	return Basis.GetUnitAxis(EAxis::X) * Clamped.Y + Basis.GetUnitAxis(EAxis::Y) * Clamped.X;
}

void UPawnCommandComponent::Move(FVector2D Direction, EGait Gait)
{
	// R5.1: Direction is clamped to length 1 for every source; the server applies no dead zone.
	MoveIntent = Direction.Size() > 1.0 ? Direction.GetSafeNormal() : Direction;
	GaitIntent = Gait;
	// The requested gait reaches the movement model (and ServerMove's FLAG_Custom_0 on an owning client).
	if (const AStealthPawn* Pawn = GetOwner<AStealthPawn>())
	{
		if (UStealthMovementComponent* Movement = Pawn->GetStealthMovement())
		{
			Movement->SetRequestedGait(Gait);
		}
	}
	const bool bHeld = !MoveIntent.IsNearlyZero();
	if (bHeld != IsComponentTickEnabled())
	{
		SetComponentTickEnabled(bHeld);
	}
}

void UPawnCommandComponent::Look(FRotator Rotation)
{
	APawn* Pawn = GetOwner<APawn>();
	if (AController* Controller = Pawn ? Pawn->GetController() : nullptr)
	{
		Controller->SetControlRotation(Rotation);
	}
}

void UPawnCommandComponent::Traverse(const FTraversalHint& Hint)
{
	FCommandPayload Payload = MakePayload();
	Payload.Arg = Hint.TraversalPointHint;
	SubmitKind(ECommandKind::Traverse, Payload);
}

void UPawnCommandComponent::InteractBegin(FCourtFixtureId Target)
{
	FCommandPayload Payload = MakePayload();
	Payload.Target = FSubjectRef::ForCourtSocket(Target.Value);
	SubmitKind(ECommandKind::InteractBegin, Payload);
}

void UPawnCommandComponent::InteractEnd()
{
	SubmitKind(ECommandKind::InteractEnd, MakePayload());
}

void UPawnCommandComponent::PrepareBegin()
{
	SubmitKind(ECommandKind::PrepareBegin, MakePayload());
}

void UPawnCommandComponent::PrepareEnd()
{
	SubmitKind(ECommandKind::PrepareEnd, MakePayload());
}

void UPawnCommandComponent::Attack(const FAttackPayload& Payload)
{
	FCommandPayload Out = MakePayload();
	Out.WorldPoint = Payload.WorldPoint;
	if (Payload.PawnRef.IsValid())
	{
		Out.Target = FSubjectRef::ForPawn(FPawnHandle{ Payload.PawnRef });
	}
	SubmitKind(ECommandKind::Attack, Out);
}

void UPawnCommandComponent::Hide(const FHideCommand& Cmd)
{
	FCommandPayload Payload = MakePayload();
	if (Cmd.Direction == EHideDirection::Enter)
	{
		Payload.Arg = Cmd.BushId;
		SubmitKind(ECommandKind::HideEnter, Payload);
	}
	else
	{
		Payload.Arg = Cmd.SideIndex;
		SubmitKind(ECommandKind::HideExit, Payload);
	}
}

FActionState UPawnCommandComponent::WriteActionState(const FActionStateWrite& Write)
{
	AStealthPawn* Pawn = GetOwner<AStealthPawn>();
	if (Pawn == nullptr || !Pawn->HasAuthority())
	{
		return Pawn != nullptr ? Pawn->GetActionState() : FActionState();
	}
	FActionState Next;
	Next.Pose = Write.Pose;
	Next.Seq = FActionState::NextSeq(Pawn->ActionState.Seq);
	Next.Variant = Write.Variant;
	Next.StartServerTime = Write.StartServerTime >= 0.0 ? Write.StartServerTime : (GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0);
	Next.ContactServerTime = Write.ContactServerTime;
	Next.WarpPoint = Write.WarpPoint;
	{
		FStealthForcedNetUpdate Forced(*Pawn, EForcedUpdateReason::ActionStateWrite);
		Pawn->ActionState = Next;
	}
	Pawn->HandleActionStateChanged();
	return Next;
}

FCommandPayload UPawnCommandComponent::MakePayload() const
{
	FCommandPayload Payload;
	Payload.Pawn.Pawn = GetOwner<APawn>();
	if (OpenOrigin)
	{
		Payload.ClientSeq = OpenOrigin->Seq;
		Payload.ControlEpoch = OpenOrigin->ControlEpoch;
	}
	return Payload;
}

void UPawnCommandComponent::SubmitKind(ECommandKind Kind, const FCommandPayload& Payload)
{
	// Bind lazily if the composition root wired the world after this pawn began play.
	if (!Core.IsValid())
	{
		if (const UActionHub* Hub = UActionHub::Get(GetWorld()))
		{
			if (const FPawnCommandCoreDeps* Deps = Hub->GetCommandWiring())
			{
				Core.Initialize(*Deps);
			}
		}
	}
	Core.Submit(OpenOrigin ? OpenOrigin->Origin : FCommandOrigin::ServerLocal(), Kind, Payload);
}

FScopedCommandOrigin::FScopedCommandOrigin(UPawnCommandComponent& InComponent, const FCommandOrigin& InOrigin, const FClientCommandSeq& InSeq, uint32 InControlEpoch)
	: Component(InComponent)
	, Origin(InOrigin)
	, Seq(InSeq)
	, ControlEpoch(InControlEpoch)
	, Previous(InComponent.OpenOrigin)
{
	Component.OpenOrigin = this;
}

FScopedCommandOrigin::~FScopedCommandOrigin()
{
	Component.OpenOrigin = Previous;
}
