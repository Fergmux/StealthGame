// Story asn-024: the owner result matcher, the plate timeline and the brace square rule. See View/OwnResultMatcher.h.

#include "View/OwnResultMatcher.h"

namespace OwnResultMatcherLocal
{
	/** A time counts as reached one millisecond early (the presentation's contact-frame tolerance). */
	constexpr double ReachedEpsilonSec = 0.001;
	/** Two contact times closer than this are the same contact. */
	constexpr double SameContactSec = 0.002;
	/** Bounded bookkeeping (a press, a write and a result are matched within about a second). */
	constexpr int32 MaxCommands = 16;
	constexpr int32 MaxObservations = 16;

	bool SeqLessOrEqual(const FClientCommandSeq& A, const FClientCommandSeq& B)
	{
		return A.PossessionEpoch != B.PossessionEpoch ? A.PossessionEpoch < B.PossessionEpoch : A.Seq <= B.Seq;
	}
}

const TCHAR* LexOwnResultPlate(EOwnResultPlate Plate)
{
	switch (Plate)
	{
	case EOwnResultPlate::TargetEliminated: return TEXT("TargetEliminated");
	case EOwnResultPlate::WrongTarget: return TEXT("WrongTarget");
	case EOwnResultPlate::NoContact: return TEXT("NoContact");
	case EOwnResultPlate::Reversed: return TEXT("Reversed");
	default: return TEXT("StrikeReversed");
	}
}

FText OwnResultPlateText(EOwnResultPlate Plate)
{
	switch (Plate)
	{
	case EOwnResultPlate::TargetEliminated: return NSLOCTEXT("Hud", "Hud.Result.RivalEliminated", "Target eliminated");
	case EOwnResultPlate::WrongTarget: return NSLOCTEXT("Hud", "Hud.Result.WrongTarget", "Wrong target");
	case EOwnResultPlate::NoContact: return NSLOCTEXT("Hud", "Hud.Result.NoContact", "No contact");
	case EOwnResultPlate::Reversed: return NSLOCTEXT("Hud", "Hud.Result.Reversed", "Reversed");
	default: return NSLOCTEXT("Hud", "Hud.Result.Defence", "Strike reversed");
	}
}

EOwnResultPlate OwnResultPlateOf(EStrikeResult Result)
{
	switch (Result)
	{
	case EStrikeResult::RivalEliminated: return EOwnResultPlate::TargetEliminated;
	case EStrikeResult::WrongTarget: return EOwnResultPlate::WrongTarget;
	case EStrikeResult::Reversed: return EOwnResultPlate::Reversed;
	default: return EOwnResultPlate::NoContact;
	}
}

const TCHAR* LexOwnResultTimeSource(EOwnResultTimeSource Source)
{
	switch (Source)
	{
	case EOwnResultTimeSource::ResultContact: return TEXT("ResultContact");
	case EOwnResultTimeSource::MatchedActionState: return TEXT("MatchedActionState");
	default: return TEXT("LostDataCap");
	}
}

void FOwnResultMatcher::OnCommandSent(ECommandKind Kind, const FClientCommandSeq& Seq, FPawnKey Pawn, const FActionState& PawnStateAtPress)
{
	if (Kind != ECommandKind::Attack && Kind != ECommandKind::PrepareBegin)
	{
		return;
	}
	FCommandRecord& Record = Commands.AddDefaulted_GetRef();
	Record.Kind = Kind;
	Record.Seq = Seq;
	Record.Pawn = Pawn;
	Record.BaselineSeq = PawnStateAtPress.Seq;
	Record.BaselineContact = PawnStateAtPress.ContactServerTime;
	if (Commands.Num() > OwnResultMatcherLocal::MaxCommands)
	{
		Commands.RemoveAt(0, Commands.Num() - OwnResultMatcherLocal::MaxCommands);
	}
}

void FOwnResultMatcher::OnRejected(const FClientCommandSeq& Seq)
{
	Commands.RemoveAll([&Seq](const FCommandRecord& Record) { return Record.Seq == Seq; });
}

const FOwnResultMatcher::FCommandRecord* FOwnResultMatcher::FindCommand(const FClientCommandSeq& Seq) const
{
	return Commands.FindByPredicate([&Seq](const FCommandRecord& Record) { return Record.Seq == Seq; });
}

bool FOwnResultMatcher::AcceptKey(ECommandReplyKind Kind, const FClientCommandSeq& Seq)
{
	const TPair<ECommandReplyKind, FClientCommandSeq> Key(Kind, Seq);
	const bool bInRing = AcceptedRing.ContainsByPredicate([&Key](const TPair<ECommandReplyKind, FClientCommandSeq>& Entry) { return Entry.Key == Key.Key && Entry.Value == Key.Value; });
	const FClientCommandSeq* Newest = NewestAccepted.Find(Kind);
	if (bInRing || (Newest != nullptr && OwnResultMatcherLocal::SeqLessOrEqual(Seq, *Newest)))
	{
		++DroppedCount;
		return false;
	}
	AcceptedRing.Add(Key);
	if (AcceptedRing.Num() > DuplicateRingSize)
	{
		AcceptedRing.RemoveAt(0, AcceptedRing.Num() - DuplicateRingSize);
	}
	NewestAccepted.Add(Kind, Seq);
	return true;
}

double FOwnResultMatcher::LastConsumedContact(FPawnKey Pawn) const
{
	const FConsumed* Consumed = LastConsumed.Find(Pawn);
	return Consumed != nullptr ? Consumed->ContactServerTime : -1.0;
}

void FOwnResultMatcher::SetShowTime(FPendingResult& Result, double ContactServerTime, EOwnResultTimeSource Source) const
{
	const double Arrival = Result.Due.ArrivalServerTime;
	Result.bTimeKnown = true;
	Result.Due.ContactServerTime = ContactServerTime;
	Result.Due.Source = Source;
	// HUD F2: t_show = min(max(t_c, t_arrival), t_arrival + HudResultPlateMaxHoldBackSec).
	Result.Due.ShowServerTime = FMath::Min(FMath::Max(ContactServerTime, Arrival), Arrival + Params.HoldBackSec());
	Result.Due.bLate = ContactServerTime < Arrival;
}

bool FOwnResultMatcher::Qualifies(const FPendingResult& Result, const FObservation& Observation) const
{
	if (Observation.bConsumed || Observation.Pawn != Result.Due.PawnKey || Observation.Pose != Result.WantPose || Observation.ContactServerTime < 0.0)
	{
		return false;
	}
	const FConsumed* Consumed = LastConsumed.Find(Observation.Pawn);
	if (Consumed != nullptr && (Observation.Seq == Consumed->Seq || Observation.ContactServerTime <= Consumed->ContactServerTime))
	{
		return false;
	}
	if (Result.bHasCommand)
	{
		// The write after the press: a different Seq and a later contact than the pawn showed at the press.
		return Observation.Seq != Result.BaselineSeq && Observation.ContactServerTime > Result.BaselineContact;
	}
	// No recorded press (lost data): only a write recent enough to belong to this result.
	return Observation.ContactServerTime >= Result.Due.ArrivalServerTime - Params.HoldBackSec();
}

void FOwnResultMatcher::Match(FPendingResult& Result, FObservation& Observation)
{
	Observation.bConsumed = true;
	FConsumed& Consumed = LastConsumed.FindOrAdd(Observation.Pawn);
	Consumed.Seq = Observation.Seq;
	Consumed.ContactServerTime = Observation.ContactServerTime;
	SetShowTime(Result, Observation.ContactServerTime, EOwnResultTimeSource::MatchedActionState);
}

void FOwnResultMatcher::TryMatch(FPendingResult& Result)
{
	for (FObservation& Observation : Observations)
	{
		if (Qualifies(Result, Observation))
		{
			Match(Result, Observation);
			return;
		}
	}
}

void FOwnResultMatcher::CheckRoleChange(FPendingResult& Result) const
{
	if (!Result.bAwaitRoleChange || Result.bRoleChangeSeen)
	{
		return;
	}
	const double* At = ReversedAgainstAt.Find(Result.Due.PawnKey);
	const double Floor = Result.bTimeKnown ? Result.Due.ContactServerTime - OwnResultMatcherLocal::SameContactSec : Result.Due.ArrivalServerTime - Params.HoldBackSec();
	Result.bRoleChangeSeen = At != nullptr && *At >= Floor;
}

void FOwnResultMatcher::AddPending(FPendingResult&& Result)
{
	if (!Result.bTimeKnown)
	{
		// Pending at the lost-data cap until its write is observed.
		Result.Due.Source = EOwnResultTimeSource::LostDataCap;
		Result.Due.ShowServerTime = Result.Due.ArrivalServerTime + Params.HoldBackSec();
	}
	CheckRoleChange(Result);
	Pending.Add(MoveTemp(Result));
}

bool FOwnResultMatcher::OnStrikeResult(const FStrikeResult& Result, double ArrivalServerTime, FPawnKey FallbackPawn)
{
	if (!AcceptKey(ECommandReplyKind::StrikeResult, Result.ClientCommandSeq))
	{
		return false;
	}
	FPendingResult Entry;
	Entry.Due.Plate = OwnResultPlateOf(Result.Result);
	Entry.Due.Kind = ECommandReplyKind::StrikeResult;
	Entry.Due.Seq = Result.ClientCommandSeq;
	Entry.Due.Strike = Result;
	Entry.Due.ArrivalServerTime = ArrivalServerTime;
	Entry.WantPose = EActionPose::Strike;
	Entry.bAwaitRoleChange = Result.Result == EStrikeResult::Reversed;
	if (const FCommandRecord* Command = FindCommand(Result.ClientCommandSeq))
	{
		Entry.bHasCommand = true;
		Entry.Due.PawnKey = Command->Pawn;
		Entry.BaselineSeq = Command->BaselineSeq;
		Entry.BaselineContact = Command->BaselineContact;
	}
	else
	{
		Entry.Due.PawnKey = FallbackPawn;
	}
	OnRejected(Result.ClientCommandSeq); // the press is answered
	if (Result.ContactServerTime >= 0.0)
	{
		// The attacker's own contact time: never an inferred one (ADR-0018 section 5.3). Its write is consumed with it.
		SetShowTime(Entry, Result.ContactServerTime, EOwnResultTimeSource::ResultContact);
		FConsumed& Consumed = LastConsumed.FindOrAdd(Entry.Due.PawnKey);
		Consumed.ContactServerTime = FMath::Max(Consumed.ContactServerTime, Result.ContactServerTime);
		for (FObservation& Observation : Observations)
		{
			if (Observation.Pawn == Entry.Due.PawnKey && Observation.Pose == EActionPose::Strike
				&& FMath::Abs(Observation.ContactServerTime - Result.ContactServerTime) < OwnResultMatcherLocal::SameContactSec)
			{
				Observation.bConsumed = true;
				Consumed.Seq = Observation.Seq;
			}
		}
	}
	else
	{
		TryMatch(Entry);
	}
	AddPending(MoveTemp(Entry));
	return true;
}

bool FOwnResultMatcher::OnDefenceResult(const FClientCommandSeq& Seq, const FDefenceResult& Result, double ArrivalServerTime, FPawnKey FallbackPawn)
{
	if (!AcceptKey(ECommandReplyKind::DefenceResult, Seq))
	{
		return false;
	}
	FPendingResult Entry;
	Entry.Due.Plate = EOwnResultPlate::StrikeReversed;
	Entry.Due.Kind = ECommandReplyKind::DefenceResult;
	Entry.Due.Seq = Seq;
	Entry.Due.ArrivalServerTime = ArrivalServerTime;
	Entry.WantPose = EActionPose::Counter;
	if (const FCommandRecord* Command = FindCommand(Seq))
	{
		Entry.bHasCommand = true;
		Entry.Due.PawnKey = Command->Pawn;
		Entry.BaselineSeq = Command->BaselineSeq;
		Entry.BaselineContact = Command->BaselineContact;
	}
	else
	{
		Entry.Due.PawnKey = FallbackPawn;
	}
	OnRejected(Seq);
	TryMatch(Entry);
	AddPending(MoveTemp(Entry));
	return true;
}

void FOwnResultMatcher::OnActionStateObserved(FPawnKey Pawn, const FActionState& State, double ServerNow)
{
	uint16& Last = LastObservedSeq.FindOrAdd(Pawn);
	if (Last == State.Seq)
	{
		return;
	}
	Last = State.Seq;
	if ((State.Pose != EActionPose::Strike && State.Pose != EActionPose::Counter) || State.ContactServerTime < 0.0)
	{
		return;
	}
	FObservation& Observation = Observations.AddDefaulted_GetRef();
	Observation.Pawn = Pawn;
	Observation.Seq = State.Seq;
	Observation.Pose = State.Pose;
	Observation.ContactServerTime = State.ContactServerTime;
	if (Observations.Num() > OwnResultMatcherLocal::MaxObservations)
	{
		Observations.RemoveAt(0, Observations.Num() - OwnResultMatcherLocal::MaxObservations);
	}
	// The other arrival order: a result already Pending on this pawn takes the write.
	FObservation& Added = Observations.Last();
	for (FPendingResult& Result : Pending)
	{
		if (!Result.bTimeKnown && Qualifies(Result, Added))
		{
			Match(Result, Added);
			CheckRoleChange(Result);
			break;
		}
	}
}

void FOwnResultMatcher::OnReversedAgainstPresented(FPawnKey Pawn, double ServerNow)
{
	ReversedAgainstAt.Add(Pawn, ServerNow);
	for (FPendingResult& Result : Pending)
	{
		CheckRoleChange(Result);
	}
}

TArray<FOwnResultDue> FOwnResultMatcher::PopDue(double ServerNow)
{
	using namespace OwnResultMatcherLocal;
	TArray<FOwnResultDue> Out;
	for (int32 Index = 0; Index < Pending.Num();)
	{
		FPendingResult& Result = Pending[Index];
		const bool bShowReached = ServerNow + ReachedEpsilonSec >= Result.Due.ShowServerTime;
		const bool bCapReached = ServerNow + ReachedEpsilonSec >= Result.Due.ArrivalServerTime + Params.HoldBackSec();
		if (bShowReached && (!Result.bAwaitRoleChange || Result.bRoleChangeSeen || bCapReached))
		{
			Result.Due.bInRoleChange = Result.bAwaitRoleChange && Result.bRoleChangeSeen;
			Out.Add(Result.Due);
			Pending.RemoveAt(Index);
			continue;
		}
		++Index;
	}
	Out.StableSort([](const FOwnResultDue& A, const FOwnResultDue& B) { return A.ShowServerTime < B.ShowServerTime; });
	return Out;
}

TArray<FOwnResultMatcher::FPawnKey> FOwnResultMatcher::GetWatchedPawns() const
{
	TArray<FPawnKey> Out;
	for (const FCommandRecord& Record : Commands)
	{
		Out.AddUnique(Record.Pawn);
	}
	for (const FPendingResult& Result : Pending)
	{
		Out.AddUnique(Result.Due.PawnKey);
	}
	return Out;
}

void FOwnResultMatcher::Reset()
{
	Commands.Reset();
	Observations.Reset();
	Pending.Reset();
	LastConsumed.Reset();
	LastObservedSeq.Reset();
	ReversedAgainstAt.Reset();
	AcceptedRing.Reset();
	NewestAccepted.Reset();
	DroppedCount = 0;
}

FOwnResultPlateSample FOwnResultPlateTimeline::Sample(const FOwnResultPlateTimelineParams& Params, double Elapsed)
{
	FOwnResultPlateSample Out;
	if (Elapsed < 0.0)
	{
		return Out;
	}
	const double In = FMath::Max(0.0, Params.InSec);
	const double Hold = FMath::Max(0.0, Params.HoldSec * Params.ReadHoldMult);
	const double Exit = FMath::Max(0.0, Params.OutSec);
	const int32 Steps = FMath::Max(1, Params.InSteps);
	if (Elapsed < In)
	{
		// Stepped entrance (art bible 7.6): whole steps, no motion.
		const int32 Step = FMath::Min(Steps, FMath::FloorToInt32(Elapsed / (In / Steps)) + 1);
		Out.Phase = EOwnResultPlatePhase::Entering;
		Out.Opacity = static_cast<float>(Step) / static_cast<float>(Steps);
		return Out;
	}
	if (Elapsed < In + Hold)
	{
		Out.Phase = EOwnResultPlatePhase::Holding;
		Out.Opacity = 1.0f;
		return Out;
	}
	if (Elapsed < In + Hold + Exit)
	{
		Out.Phase = EOwnResultPlatePhase::Exiting;
		Out.Opacity = static_cast<float>(1.0 - (Elapsed - In - Hold) / Exit);
		return Out;
	}
	Out.Phase = EOwnResultPlatePhase::Gone;
	return Out;
}

bool FOwnBraceIndicator::IsShown(const FActionState& OwnState, double ServerNow)
{
	if (OwnState.Pose == EActionPose::Brace)
	{
		return true;
	}
	return OwnState.Pose == EActionPose::Counter && OwnState.ContactServerTime >= 0.0
		&& ServerNow + OwnResultMatcherLocal::ReachedEpsilonSec < OwnState.ContactServerTime;
}
