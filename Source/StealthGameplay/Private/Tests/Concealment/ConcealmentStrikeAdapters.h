#pragma once

// Test-only adapters (stories con-015, con-016): Assassination's victim resolver read against the REAL Concealment core of a
// StealthConcealmentTest::FRig: the core as IBushSpotQuery, and the rig's scripted pawns and roster as the resolver's pawn
// query and roster gateway. The pick itself is Assassination's FVictimResolverCore (the BushSpotPick stream, asn-006).

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ConcealmentTestRig.h"
#include "Server/BushSpotQuery.h"
#include "Server/EncounterTypes.h"
#include "Server/Random/StealthServerSeeds.h"
#include "Server/VictimResolverCore.h"

namespace StealthConcealmentTest
{
	/** IBushSpotQuery over the core (the service's own forwarding, without a world subsystem). */
	class FCoreSpotQuery final : public IBushSpotQuery
	{
	public:
		FConcealmentCore* Core = nullptr;
		virtual bool FindSpotZone(const FVector& WorldPoint, uint16& OutBushId) const override
		{
			const TOptional<uint16> Bush = Core->TargetsBush(WorldPoint);
			OutBushId = Bush.Get(0);
			return Bush.IsSet();
		}
		virtual void GetSpotOccupants(uint16 BushId, const APawn* Attacker, TArray<APawn*>& OutOccupants) const override { Core->GetSpotOccupants(BushId, Attacker, OutOccupants); }
		virtual bool IsConcealed(const APawn* Pawn) const override { return Core->IsConcealed(Pawn); }
		virtual bool GetFootprint(uint16 BushId, FBox2D& OutFootprint) const override
		{
			const FBushRuntime* Bush = Core->FindBush(BushId);
			if (Bush != nullptr)
			{
				OutFootprint = Bush->Footprint;
			}
			return Bush != nullptr;
		}
		virtual void RemoveOccupant(APawn* Pawn) override { Core->OnPawnKilled(*Pawn, FQueueKey()); }
	};

	/** The production rule (FEncounterConcealmentVisibility): a concealed pawn is not knowable through a PawnRef. */
	class FCoreVisibility final : public IEncounterVisibilityQuery
	{
	public:
		FConcealmentCore* Core = nullptr;
		virtual bool CanViewerKnow(const APawn*, const APawn* Pawn) override { return Pawn != nullptr && !Core->IsConcealed(Pawn); }
	};

	/** IEncounterPawnQuery over the rig's scripted pawns. */
	class FRigPawnQuery final : public IEncounterPawnQuery
	{
	public:
		FRig* Rig = nullptr;
		virtual void GetPawns(TArray<FEncounterPawnFacts>& OutPawns) const override
		{
			OutPawns.Reset();
			uint64 Serial = 1;
			for (const TPair<const APawn*, FPawnFacts>& Pair : Rig->Pawns.Facts)
			{
				FEncounterPawnFacts Facts;
				Facts.Pawn = const_cast<APawn*>(Pair.Key);
				Facts.Location = Pair.Value.Location;
				Facts.SpawnSerial = Serial++;
				OutPawns.Add(Facts);
			}
		}
		virtual bool GetFacts(const APawn* Pawn, FEncounterPawnFacts& OutFacts) const override
		{
			TArray<FEncounterPawnFacts> All;
			GetPawns(All);
			const FEncounterPawnFacts* Found = All.FindByPredicate([Pawn](const FEncounterPawnFacts& F) { return F.Pawn == Pawn; });
			if (Found != nullptr)
			{
				OutFacts = *Found;
			}
			return Found != nullptr;
		}
	};

	/** IEncounterRosterGateway over the rig's scripted relations (teams; unbound = NPC). */
	class FRigRoster final : public IEncounterRosterGateway
	{
	public:
		FRig* Rig = nullptr;
		virtual FRosterVictimKind ResolveVictimKind(const APawn* Pawn) override
		{
			return FRosterVictimKind::Make(Rig->Relations.GetParticipant(*Pawn).IsValid() ? ERosterVictimKind::Player : ERosterVictimKind::Npc);
		}
		virtual bool AreTeammates(const APawn* A, const APawn* B) override
		{
			const FParticipantId PA = Rig->Relations.GetParticipant(*A);
			return PA.IsValid() && Rig->Relations.IsAllyViewer(PA, *B);
		}
		virtual bool IsLegalityActive(const APawn*) override { return true; }
		virtual void RequestElimination(const APawn*, ERosterReason, double) override {}
		virtual FParticipantId GetParticipant(const APawn* Pawn) override { return Rig->Relations.GetParticipant(*Pawn); }
		virtual uint32 GetTeamIndex(const APawn* Pawn) override
		{
			const FParticipantId P = Rig->Relations.GetParticipant(*Pawn);
			const int32* Team = P.IsValid() ? Rig->Relations.Teams.Find(P.Value) : nullptr;
			return Team != nullptr ? static_cast<uint32>(*Team) : 0xFFu;
		}
	};

	/** A sub-seed source that answers one injected seed (and records the domains asked). */
	class FInjectedSeeds final : public IServerSubSeedSource
	{
	public:
		uint64 Seed = 1;
		mutable TArray<ESeedDomain> Asked;
		virtual uint64 GetServerSubSeed(ESeedDomain Domain, uint32 Attempt) const override { Asked.Add(Domain); return Seed ^ (static_cast<uint64>(Attempt) << 32); }
	};

	/** Everything the resolver needs over one rig. */
	struct FStrikeView
	{
		FCoreSpotQuery Spots;
		FCoreVisibility Visibility;
		FRigPawnQuery PawnQuery;
		FRigRoster Roster;
		FInjectedSeeds Seeds;
		FEncounterParams Params = FEncounterParams::Defaults();

		explicit FStrikeView(FRig& Rig)
		{
			Spots.Core = Rig.Core.Get();
			Visibility.Core = Rig.Core.Get();
			PawnQuery.Rig = &Rig;
			Roster.Rig = &Rig;
		}

		FVictimResolverDeps Deps() const
		{
			FVictimResolverDeps Out;
			Out.Pawns = &PawnQuery;
			Out.Roster = const_cast<FRigRoster*>(&Roster);
			Out.Bushes = &Spots;
			Out.Visibility = const_cast<FCoreVisibility*>(&Visibility);
			Out.SubSeeds = &Seeds;
			return Out;
		}

		/** One strike's resolution: Attacker aims at WorldPoint (optional PawnRef), keyed Key. */
		FVictimResolution Strike(const APawn* Attacker, const FVector& WorldPoint, APawn* Ref, FQueueKey Key) const
		{
			FVictimRequest Request;
			PawnQuery.GetFacts(Attacker, Request.Attacker);
			Request.WorldPoint = WorldPoint;
			Request.PawnRef = Ref;
			Request.Key = Key;
			return FVictimResolverCore::Resolve(Params, Deps(), Request);
		}
	};
}

#endif
