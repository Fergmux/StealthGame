// Stories asn-005, 007, 008, 009, 010 (production/epics/assassination-defence-reversal/): the strike-role table and the
// F2 braced-at-the-Attack key compare. See Server/StrikeRoleTable.h.

#include "Server/StrikeRoleTable.h"

#include "Server/DefenceStateCore.h"

EStrikeRole FStrikeRoleTable::GetRole(const APawn* Pawn, double NowServerTime, const FDefenceStateCore* Defence, double CooldownSec) const
{
	const FEntry* Entry = Find(Pawn);
	const bool bCooling = Defence != nullptr && !Defence->CanPrepare(NowServerTime, CooldownSec);
	if (Entry == nullptr)
	{
		return bCooling ? EStrikeRole::Cooling : EStrikeRole::Ready;
	}
	switch (Entry->Role)
	{
	case EStrikeRole::Dead:
	case EStrikeRole::Bracing:
		return Entry->Role;
	case EStrikeRole::Striking:
	case EStrikeRole::Countering:
		if (NowServerTime < Entry->RoleEndServerTime)
		{
			return Entry->Role;
		}
		break;
	default:
		break;
	}
	return bCooling ? EStrikeRole::Cooling : EStrikeRole::Ready;
}

void FStrikeRoleTable::SetBracing(const APawn* Pawn, FReplySinkId BraceSink)
{
	FEntry& Entry = FindOrAdd(Pawn);
	Entry.Role = EStrikeRole::Bracing;
	Entry.RoleEndServerTime = 0.0;
	Entry.BraceSink = BraceSink;
}

void FStrikeRoleTable::EndBrace(const APawn* Pawn)
{
	if (FEntry* Entry = Entries.Find(FObjectKey(Pawn)))
	{
		if (Entry->Role == EStrikeRole::Bracing)
		{
			Entry->Role = EStrikeRole::Cooling;
		}
	}
}

void FStrikeRoleTable::SetStriking(const APawn* Pawn, double LockoutEndServerTime)
{
	FEntry& Entry = FindOrAdd(Pawn);
	if (Entry.Role != EStrikeRole::Dead)
	{
		Entry.Role = EStrikeRole::Striking;
		Entry.RoleEndServerTime = LockoutEndServerTime;
	}
}

void FStrikeRoleTable::SetCountering(const APawn* Pawn, double CounterEndServerTime)
{
	FEntry& Entry = FindOrAdd(Pawn);
	if (Entry.Role != EStrikeRole::Dead)
	{
		Entry.Role = EStrikeRole::Countering;
		Entry.RoleEndServerTime = CounterEndServerTime;
	}
}

void FStrikeRoleTable::SetDead(const APawn* Pawn, double DeathServerTime)
{
	FEntry& Entry = FindOrAdd(Pawn);
	Entry.Role = EStrikeRole::Dead;
	Entry.DeathServerTime = DeathServerTime;
}

bool FStrikeRoleTable::IsDead(const APawn* Pawn) const
{
	const FEntry* Entry = Find(Pawn);
	return Entry != nullptr && Entry->Role == EStrikeRole::Dead;
}

TOptional<double> FStrikeRoleTable::GetScheduledDeathServerTime(const APawn* Pawn) const
{
	const FEntry* Entry = Find(Pawn);
	return Entry != nullptr && Entry->Role == EStrikeRole::Dead ? TOptional<double>(Entry->DeathServerTime) : TOptional<double>();
}

FReplySinkId FStrikeRoleTable::GetBraceSink(const APawn* Pawn) const
{
	const FEntry* Entry = Find(Pawn);
	return Entry != nullptr ? Entry->BraceSink : FReplySinkId();
}

double FStrikeRoleTable::GetRoleEnd(const APawn* Pawn) const
{
	const FEntry* Entry = Find(Pawn);
	return Entry != nullptr ? Entry->RoleEndServerTime : 0.0;
}

bool FBracedAtAttackCore::IsBraced(EStrikeRole RoleAtSlot, const FDefenceStateCore* Defence, FQueueKey AttackKey)
{
	return RoleAtSlot == EStrikeRole::Bracing && Defence != nullptr && Defence->IsWindowOpenAt(AttackKey);
}
