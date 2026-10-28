#pragma once

// Story oc-013's fixture layout (test-only): twelve courts with public ClueTag records, each clue shared by at most three
// courts, and a pinned list of 1,000 assignments (five distinct courts per assignment, drawn by a fixed xorshift from the
// seeds 1..1000, so the list is the same on every run and platform). The graybox layout's real records are the Level Socket
// Contract's (not built): AC1 on the graybox waits for them.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/ObjectivesSeams.h"

namespace StealthObjectivesClueFixture
{
	/** Court ids 101..112. */
	inline constexpr int32 CourtCount = 12;
	inline constexpr uint16 FirstCourt = 101;
	inline constexpr int32 AssignmentCount = 1000;

	/** Each court's (area, feature): every clue appears 1-3 times. */
	inline void Records(TArray<FCourtClueRecord>& Out)
	{
		const uint8 Tags[CourtCount][2] = {
			{ 2, 1 }, { 2, 1 }, { 2, 5 },          // market: water steps x2, stalls
			{ 3, 2 }, { 3, 2 }, { 3, 2 },          // temple: shrine x3
			{ 1, 3 }, { 1, 0 },                    // causeway: stairway, area only
			{ 4, 4 }, { 4, 6 }, { 4, 6 }, { 4, 0 } // residential: well, great tree x2, area only
		};
		Out.Reset();
		for (int32 Index = 0; Index < CourtCount; ++Index)
		{
			FCourtClueRecord& Record = Out.AddDefaulted_GetRef();
			Record.Court = FCourtFixtureId(static_cast<uint16>(FirstCourt + Index));
			Record.AreaTypeId = Tags[Index][0];
			Record.NearestFeatureKind = Tags[Index][1];
		}
	}

	/** xorshift32 (Marsaglia), never 0. */
	inline uint32 Next(uint32& State)
	{
		State ^= State << 13;
		State ^= State >> 17;
		State ^= State << 5;
		return State;
	}

	/** Pinned assignment Index (0..999): five distinct courts as P1[0], P1[1], P2[0], P2[1], Final. */
	inline FSiteAssignment Assignment(int32 Index)
	{
		uint32 State = static_cast<uint32>(Index + 1) * 2654435761u;
		State = State != 0 ? State : 1u;
		TArray<uint16> Pool;
		for (int32 Court = 0; Court < CourtCount; ++Court)
		{
			Pool.Add(static_cast<uint16>(FirstCourt + Court));
		}
		for (int32 Pick = Pool.Num() - 1; Pick > 0; --Pick)
		{
			Pool.Swap(Pick, static_cast<int32>(Next(State) % static_cast<uint32>(Pick + 1)));
		}
		FSiteAssignment Out;
		Out.P1[0] = FCourtFixtureId(Pool[0]);
		Out.P1[1] = FCourtFixtureId(Pool[1]);
		Out.P2[0] = FCourtFixtureId(Pool[2]);
		Out.P2[1] = FCourtFixtureId(Pool[3]);
		Out.Final = FCourtFixtureId(Pool[4]);
		return Out;
	}

	/** The same five courts with the roles rotated (P1[0] <- Final, P1[1] <- P1[0], ...): a role perturbation. */
	inline FSiteAssignment RotateRoles(const FSiteAssignment& In)
	{
		FSiteAssignment Out;
		Out.P1[0] = In.Final;
		Out.P1[1] = In.P1[0];
		Out.P2[0] = In.P1[1];
		Out.P2[1] = In.P2[0];
		Out.Final = In.P2[1];
		return Out;
	}
}

#endif
