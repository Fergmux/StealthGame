#include "Server/Core/CommandIngress.h"

#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Server/Core/StealthPawnCommandComponent.h"
#include "Server/Testing/StealthCommandSeams.h"
#include "View/Core/StealthPawn.h"

APawn* ResolvePawnRefGuid(const UWorld* World, const FNetworkGUID& Guid)
{
	// The default GUID (ObjectId 1) is never a pawn, and the engine's lookup ensures on it (spike V3 finding, case e1):
	// a client could send it, so it is refused here before the cache is asked.
	if (!Guid.IsValid() || Guid.IsDefault() || World == nullptr)
	{
		return nullptr;
	}
	const UNetDriver* Driver = World->GetNetDriver();
	if (Driver == nullptr || !Driver->GetNetGuidCache().IsValid())
	{
		return nullptr;
	}
	// bIgnoreMustBeMapped = false: the server resolves what it assigned (spike V3 records the cases). The cache still
	// answers a destroyed actor until garbage collection (V3 case d), so a torn-down pawn resolves as absent.
	APawn* Pawn = Cast<APawn>(Driver->GetNetGuidCache()->GetObjectFromNetGUID(Guid, false));
	return IsValid(Pawn) && !Pawn->IsActorBeingDestroyed() ? Pawn : nullptr;
}

ECommandIngressResult IngestCommand(APlayerController& Owner, const FCommandIngressContext& Context, const FNetPawnCommand& Cmd)
{
	if (!Cmd.IsStructurallyValid())
	{
		return ECommandIngressResult::DroppedMalformed;
	}
	// The controller's own possessed pawn: the client never names the pawn it commands.
	AStealthPawn* Pawn = Cast<AStealthPawn>(Owner.GetPawn());
	UPawnCommandComponent* Component = Pawn ? Pawn->GetPawnCommand() : nullptr;
	if (Component == nullptr)
	{
		return ECommandIngressResult::DroppedNoPawn;
	}

#if WITH_STEALTH_TEST_SEAMS
	if (StealthCommandSeams::HasObserver())
	{
		FStealthCommandLogEntry Entry;
		Entry.Point = EStealthCommandLogPoint::IngressOutput;
		Entry.Kind = Cmd.Kind;
		Entry.Seq = Cmd.Seq;
		Entry.ArrivalTick = GFrameCounter;
		Entry.Sink = Context.Origin.Sink;
		Entry.Mailbox = Context.Origin.Mailbox;
		Entry.Args = Cmd.Args;
		StealthCommandSeams::Notify(Entry);
	}
#endif

	FScopedCommandOrigin Scope(*Component, Context.Origin, Cmd.Seq, Context.ControlEpoch);
	switch (Cmd.Kind)
	{
	case ECommandKind::Traverse:
	{
		FTraversalHint Hint;
		Hint.TraversalPointHint = Cmd.Args.TraversalPointHint;
		Hint.ClientTimestamp = Cmd.Args.ClientTimestamp;
		Component->Traverse(Hint);
		break;
	}
	case ECommandKind::InteractBegin:
		Component->InteractBegin(Cmd.Args.CourtFixtureId);
		break;
	case ECommandKind::InteractEnd:
		Component->InteractEnd();
		break;
	case ECommandKind::PrepareBegin:
		Component->PrepareBegin();
		break;
	case ECommandKind::PrepareEnd:
		Component->PrepareEnd();
		break;
	case ECommandKind::Attack:
	{
		FAttackPayload Payload;
		Payload.WorldPoint = Cmd.Args.WorldPoint;
		APawn* Ref = Context.HostLocalPawnRef.IsValid() ? Context.HostLocalPawnRef.Get() : ResolvePawnRefGuid(Owner.GetWorld(), Cmd.Args.GetPawnRefGuid());
		// A PawnRef naming the commanding pawn is stripped (counting it is story iif-020). A resolved pawn grants nothing:
		// only CanViewerKnow and PawnRefAgreementCm admit it at the slot.
		Payload.PawnRef = Ref != Pawn ? Ref : nullptr;
		Component->Attack(Payload);
		break;
	}
	case ECommandKind::HideEnter:
		Component->Hide(FHideCommand::Enter(Cmd.Args.BushId));
		break;
	case ECommandKind::HideExit:
		Component->Hide(FHideCommand::Exit(Cmd.Args.SideIndex));
		break;
	}
	return ECommandIngressResult::Submitted;
}
