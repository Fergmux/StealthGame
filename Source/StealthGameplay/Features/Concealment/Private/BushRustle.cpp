// Story con-029: the bush rustle pulse ring. See View/BushRustle.h.

#include "View/BushRustle.h"

FSoftObjectPath BushRustle::DefaultSound()
{
	return FSoftObjectPath(TEXT("/Game/Audio/Foley/MetaSounds/MSS_FoleySound_Scuff.MSS_FoleySound_Scuff"));
}

TOptional<EBushRustleKind> BushRustle::KindOfPose(const FActionState& Action)
{
	switch (Action.Pose)
	{
	case EActionPose::BushEnter: return EBushRustleKind::Enter;
	case EActionPose::BushExit: return EBushRustleKind::Exit;
	case EActionPose::Strike:
		if ((Action.Variant & StrikeVariant::BushPreface) != 0)
		{
			return EBushRustleKind::Emerge;
		}
		return {};
	default:
		return {};
	}
}

double BushRustle::OnsetFor(EBushRustleKind Kind, double StartServerTime, const FConcealmentTuning_Public& Tuning)
{
	switch (Kind)
	{
	case EBushRustleKind::Enter: return StartServerTime + Tuning.BushEnterSwapSec - OnsetLeadSec;
	case EBushRustleKind::Exit: return StartServerTime + Tuning.BushExitCrossSec - OnsetLeadSec;
	case EBushRustleKind::Emerge: return StartServerTime + Tuning.BushStrikeEmergeSec - OnsetLeadSec;
	}
	return StartServerTime;
}

FVector BushRustle::FacePoint(const FBushRuntime& Bush, uint8 Side)
{
	return Bush.Anchor + ConcealmentGeometry::SideNormal(Side) * (ConcealmentGeometry::FootprintSizeCm * 0.5);
}

FBushRustlePulse BushRustle::MakePulse(const FBushRuntime& Bush, uint8 Side, EBushRustleKind Kind, double StartServerTime,
	const FConcealmentTuning_Public& Tuning, const FSoftObjectPath& Sound)
{
	FBushRustlePulse Pulse;
	Pulse.BushId = Bush.Id;
	Pulse.Side = Side;
	Pulse.Kind = Kind;
	Pulse.StartServerTime = StartServerTime;
	Pulse.OnsetServerTime = OnsetFor(Kind, StartServerTime, Tuning);
	Pulse.WorldPos = FacePoint(Bush, Side);
	const FVector Normal = ConcealmentGeometry::SideNormal(Side);
	Pulse.SideDir = FVector2D(Normal.X, Normal.Y);
	Pulse.AmplitudeCm = Tuning.BushRustleAmplitudeCm;
	Pulse.Sound = Sound;
	Pulse.bEyeMasked = true;
	return Pulse;
}

int32 FBushRustleRing::Write(const FBushRustlePulse& Pulse)
{
	const int32 Slot = Next;
	Slots[Slot] = Pulse;
	bWritten[Slot] = true;
	Next = (Next + 1) % BushRustle::SlotCount;
	++Writes;
	return Slot;
}

const FBushRustlePulse* FBushRustleRing::GetSlot(int32 Index) const
{
	return Index >= 0 && Index < BushRustle::SlotCount && bWritten[Index] ? &Slots[Index] : nullptr;
}
