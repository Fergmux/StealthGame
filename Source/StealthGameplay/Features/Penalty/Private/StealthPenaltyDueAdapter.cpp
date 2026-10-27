#include "Server/StealthPenaltyDueAdapter.h"

#include "Server/StealthPenaltyService.h"

// Story nkp-011 (ADR-0019 section 6): one owner-part write per change, none while latched, none per frame.

FPenaltyDueAdapter::FPenaltyDueAdapter(IPenaltyOwnerViewFeed* InFeed, TFunction<bool()> InIsLatched, TFunction<uint32()> InCurrentEpoch)
	: Feed(InFeed)
	, IsLatched(MoveTemp(InIsLatched))
	, CurrentEpoch(MoveTemp(InCurrentEpoch))
{
}

FPenaltyDueAdapter::~FPenaltyDueAdapter()
{
	Unbind();
}

void FPenaltyDueAdapter::Bind(UPenaltyService& InService)
{
	Unbind();
	Service = &InService;
	Handle = InService.OnDueTimeChanged.AddRaw(this, &FPenaltyDueAdapter::OnDueTimeChanged);
}

void FPenaltyDueAdapter::Unbind()
{
	if (UPenaltyService* Bound = Service.Get())
	{
		Bound->OnDueTimeChanged.Remove(Handle);
	}
	Service.Reset();
	Handle.Reset();
}

void FPenaltyDueAdapter::OnDueTimeChanged(FParticipantId Participant, TOptional<double> DueServerTime)
{
	if (Feed == nullptr || !Participant.IsValid() || (IsLatched && IsLatched()))
	{
		return; // latched: the owner part is frozen after the deciding slot (the countdown is never published there)
	}
	Feed->SetOwnerRespawnDue(Participant, DueServerTime.Get(0.0), CurrentEpoch ? CurrentEpoch() : 0);
	++WriteCount;
}
