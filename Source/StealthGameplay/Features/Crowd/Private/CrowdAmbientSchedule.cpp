// Stories cnb-026 (ambient schedule) and cnb-027 (kill vacancy, replacement delay and exit cancellation): see
// CrowdAmbientSchedule.h (GDD CNB-R9, F3; ADR-0016 section 6).

#include "Server/CrowdAmbientSchedule.h"

#include "Server/CrowdDraws.h"
#include "Server/Random/StealthSecretStream.h"
#include "Server/StealthTuningInvariants.h"
#include "Server/StealthTuningSubstructs.h"
#include "View/Random/StealthRandomTag.h"

namespace CrowdAmbientLocal
{
	/** The schedule stream tag (each event's own stream over GetServerSubSeed(AmbientSchedule, counter)). */
	constexpr uint64 ScheduleStreamTag = StealthRandom::Tag("CNB_AMBI");
	/** Kill-cancel events take counters from this offset (their own counter space). */
	constexpr uint32 KillCounterBase = 0x80000000u;
	/** LSC EmergenceRetrySec (a pending owner key until the LSC epic registers it): a blocked entry retries after it. */
	constexpr double EmergenceRetrySec = 1.0;
	/** Draw indices inside one event's stream. */
	enum EIndex : uint32 { Gap = 0, Socket = 1, Pick = 2, CycleGap = 3, Cancel = 4, FallbackSocket = 5 };
}

double FCrowdAmbientSchedule::Unit(uint32 Counter, uint32 Index) const
{
	const uint64 SubSeed = SubSeedOf ? SubSeedOf(Counter) : (0xA3B1E5ull ^ (static_cast<uint64>(Counter) * 0x9E3779B97F4A7C15ull));
	const FSecretStream Stream(SubSeed, CrowdAmbientLocal::ScheduleStreamTag);
	const uint64 Tuple[] = { static_cast<uint64>(Index) };
	return FCrowdDraws::UnitFraction53(Stream.Draw(Tuple));
}

void FCrowdAmbientSchedule::Start(double Now, int32 SocketCount, const FCrowdTuning_Server& Tuning, uint32 InEpoch, ICrowdScheduleQueue& InQueue, TFunction<uint64(uint32)> InSubSeedOf)
{
	Queue = &InQueue;
	SubSeedOf = MoveTemp(InSubSeedOf);
	Sockets = FMath::Max(SocketCount, 0);
	Epoch = InEpoch;
	Lambda = Sockets / FMath::Max(static_cast<double>(Tuning.CrowdAmbientCycleIntervalSec), 1.0);
	GapMin = Tuning.CrowdCycleGapMinSec;
	GapMax = Tuning.CrowdCycleGapMaxSec;
	MinDelay = Tuning.CrowdReplacementMinDelaySec;
	MaxDelay = Tuning.CrowdReplacementMaxDelaySec;
	RetrySec = CrowdAmbientLocal::EmergenceRetrySec;
	CancelMaxPct = StealthTuningPendingOwnerKeys::EmergenceExitCancelMaxPct;
	ExitsPerSocket.Init(0, Sockets);
	EntriesPerSocket.Init(0, Sockets);
	LastExitDue = Now;
	bRunning = Sockets > 0;
	Tick(Now);
}

void FCrowdAmbientSchedule::ScheduleNextExit()
{
	const uint32 Counter = NextCounter++;
	const double Gap = -FMath::Loge(1.0 - Unit(Counter, CrowdAmbientLocal::Gap)) / FMath::Max(Lambda, 1e-9);
	FCrowdScheduleItem Item;
	Item.Id = NextItemId++;
	Item.Kind = ECrowdScheduleKind::Exit;
	Item.Due = LastExitDue + Gap;
	Item.Socket = FMath::Min(static_cast<int32>(Unit(Counter, CrowdAmbientLocal::Socket) * Sockets), Sockets - 1);
	Item.Counter = Counter;
	Item.Epoch = Epoch;
	LastExitDue = Item.Due;
	Items.Add(Item.Id, Item);
	FExitSlot& Slot = Exits.AddDefaulted_GetRef();
	Slot.ItemId = Item.Id;
	Slot.Due = Item.Due;
	Slot.Socket = Item.Socket;
	ExitTimes.Add(Item.Due);
	Queue->Submit(Item);
}

void FCrowdAmbientSchedule::SubmitEntry(double Due, int32 Socket, uint32 Counter, int32 Cycle, int32 Vacancy, int32 Retries)
{
	FCrowdScheduleItem Item;
	Item.Id = NextItemId++;
	Item.Kind = ECrowdScheduleKind::Entry;
	Item.Due = Due;
	Item.Socket = Socket;
	Item.Counter = Counter;
	Item.Epoch = Epoch;
	Item.Cycle = Cycle;
	Item.Vacancy = Vacancy;
	Item.Retries = Retries;
	Items.Add(Item.Id, Item);
	Queue->Submit(Item);
}

void FCrowdAmbientSchedule::Tick(double Now)
{
	if (!bRunning)
	{
		return;
	}
	while (LastExitDue < Now + HorizonSec)
	{
		ScheduleNextExit();
	}
	// The fallback replacement (CNB-R9.2): no entry has served the vacancy by CrowdReplacementMaxDelaySec.
	for (int32 Index = 0; Index < Vacancies.Num(); ++Index)
	{
		const FCrowdKillVacancy& Vacancy = Vacancies[Index];
		if (Vacancy.ServedTime < 0.0 && !FallbackScheduled.Contains(Index) && Now >= Vacancy.KillTime + MaxDelay)
		{
			FallbackScheduled.Add(Index);
			const uint32 Counter = CrowdAmbientLocal::KillCounterBase + static_cast<uint32>(Index);
			const int32 Socket = FMath::Min(static_cast<int32>(Unit(Counter, CrowdAmbientLocal::FallbackSocket) * Sockets), Sockets - 1);
			SubmitEntry(Now, Socket, Counter, INDEX_NONE, Index, 0);
		}
	}
}

int32 FCrowdAmbientSchedule::ServeableVacancy(double Now) const
{
	for (int32 Index = 0; Index < Vacancies.Num(); ++Index)
	{
		const FCrowdKillVacancy& Vacancy = Vacancies[Index];
		if (Vacancy.ServedTime < 0.0 && !FallbackScheduled.Contains(Index) && Now >= Vacancy.KillTime + MinDelay - 1e-9)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void FCrowdAmbientSchedule::Resolve(uint32 ItemId, ECrowdScheduleResult Result, double Now, ICrowdScheduleHost& Host)
{
	FCrowdScheduleItem Item;
	if (!Items.RemoveAndCopyValue(ItemId, Item))
	{
		return;
	}
	if (Result != ECrowdScheduleResult::Fired || !bRunning)
	{
		// Rejected_Latched, Rejected_StaleEpoch and Voided(Revalidation) items are never rescheduled.
		++RejectedItems;
		return;
	}
	if (Item.Kind == ECrowdScheduleKind::Exit)
	{
		if (FExitSlot* Slot = Exits.FindByPredicate([ItemId](const FExitSlot& S) { return S.ItemId == ItemId; }))
		{
			if (Slot->bCancelled)
			{
				++RejectedItems;
				return;
			}
			Slot->bFired = true;
		}
		++ExitsPerSocket[Item.Socket];
		const int32 Cycle = NextCycle++;
		// The cycle is open before the host is asked (an exiter may be absorbed inside the call).
		OpenCycles.Add(Cycle, TPair<uint32, int32>(Item.Counter, Item.Socket));
		if (!Host.StartAmbientExit(Item.Socket, Cycle, Unit(Item.Counter, CrowdAmbientLocal::Pick), Now))
		{
			OpenCycles.Remove(Cycle); // no eligible NPC: the slot passes
		}
		return;
	}
	// An entry: LSC's clear check, else the retry.
	if (!Host.IsSocketClear(Item.Socket, Now))
	{
		++RetryCount;
		SubmitEntry(Now + RetrySec, Item.Socket, Item.Counter, Item.Cycle, Item.Vacancy, Item.Retries + 1);
		return;
	}
	Host.RequestEmergence(Item.Socket, Now);
	++EntriesPerSocket[Item.Socket];
	const int32 Serve = Item.Vacancy != INDEX_NONE ? Item.Vacancy : ServeableVacancy(Now);
	if (Vacancies.IsValidIndex(Serve) && Vacancies[Serve].ServedTime < 0.0)
	{
		Vacancies[Serve].ServedTime = Now;
		Vacancies[Serve].bFallback = Item.Vacancy != INDEX_NONE;
	}
}

void FCrowdAmbientSchedule::NoteAbsorbed(int32 Cycle, double Now)
{
	TPair<uint32, int32> Open;
	if (!bRunning || !OpenCycles.RemoveAndCopyValue(Cycle, Open))
	{
		return;
	}
	const double Gap = GapMin + Unit(Open.Key, CrowdAmbientLocal::CycleGap) * (GapMax - GapMin);
	SubmitEntry(Now + Gap, Open.Value, Open.Key, Cycle, INDEX_NONE, 0);
}

void FCrowdAmbientSchedule::NoteCycleAbandoned(int32 Cycle)
{
	OpenCycles.Remove(Cycle);
}

int32 FCrowdAmbientSchedule::NoteKill(double KillTime, bool bExitingInAlley, int32 VictimCycle)
{
	FCrowdKillVacancy& Vacancy = Vacancies.AddDefaulted_GetRef();
	const int32 Index = Vacancies.Num() - 1;
	Vacancy.KillTime = KillTime;
	if (!bRunning)
	{
		return Index;
	}
	if (bExitingInAlley)
	{
		// The exiting NPC died inside its alley: a vacancy and no cancel; its cycle's entry still follows (the death takes
		// the absorption's place, so the city's entry rate stays the Poisson rate F3 assumes).
		Vacancy.bInAlley = true;
		NoteAbsorbed(VictimCycle, KillTime);
		return Index;
	}
	// One exit among those scheduled in [t_k, t_k + 60 s] at all sockets, uniformly; LSC R5.7 caps the cancelled share.
	TArray<int32, TInlineAllocator<16>> Candidates;
	int32 InWindow = 0, CancelledInWindow = 0;
	for (int32 Slot = 0; Slot < Exits.Num(); ++Slot)
	{
		const FExitSlot& Exit = Exits[Slot];
		if (Exit.Due < KillTime || Exit.Due > KillTime + CancelWindowSec)
		{
			continue;
		}
		++InWindow;
		CancelledInWindow += Exit.bCancelled ? 1 : 0;
		if (!Exit.bCancelled && !Exit.bFired)
		{
			Candidates.Add(Slot);
		}
	}
	if (Candidates.Num() == 0 || 100.0 * (CancelledInWindow + 1) > CancelMaxPct * InWindow)
	{
		++CancelsRefusedByBound;
		return Index;
	}
	const uint32 Counter = CrowdAmbientLocal::KillCounterBase + static_cast<uint32>(Index);
	const int32 Pick = Candidates[FMath::Min(static_cast<int32>(Unit(Counter, CrowdAmbientLocal::Cancel) * Candidates.Num()), Candidates.Num() - 1)];
	FExitSlot& Exit = Exits[Pick];
	Exit.bCancelled = true;
	Vacancy.CancelledExit = Exit.ItemId;
	++CancelledExits;
	CancelledSockets.Add(Exit.Socket);
	// The cancelled exit's NPC stays; its cycle's entry still comes, G after the slot (the kill's vacancy takes the
	// exit's place in the cycle), so entries stay a Poisson process of rate lambda and F3's delay model holds.
	if (const FCrowdScheduleItem* Item = Items.Find(Exit.ItemId))
	{
		SubmitEntry(Exit.Due + GapMin + Unit(Item->Counter, CrowdAmbientLocal::CycleGap) * (GapMax - GapMin), Exit.Socket, Item->Counter, INDEX_NONE, INDEX_NONE, 0);
	}
	Queue->Void(Exit.ItemId);
	return Index;
}

double FCrowdAmbientSchedule::GetExitDueOf(uint32 ItemId) const
{
	const FExitSlot* Slot = Exits.FindByPredicate([ItemId](const FExitSlot& S) { return S.ItemId == ItemId; });
	return Slot != nullptr ? Slot->Due : -1.0;
}

void FCrowdAmbientSchedule::Halt()
{
	bRunning = false;
	OpenCycles.Reset();
}
