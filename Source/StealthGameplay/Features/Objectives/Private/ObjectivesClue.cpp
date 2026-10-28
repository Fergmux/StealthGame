// Objectives Chain story oc-013: the role-blind clue function. See ObjectivesClue.h.

#include "ObjectivesClue.h"

FClueTag FObjectivesClue::ClueOf(const FCourtClueRecord& Record)
{
	FClueTag Out;
	Out.AreaType = Record.AreaTypeId <= GClueAreaTypeMax ? Record.AreaTypeId : 0;
	Out.FeatureKind = Record.NearestFeatureKind <= GClueFeatureKindMax ? Record.NearestFeatureKind : 0;
	return Out;
}

const FCourtClueRecord* FObjectivesClue::FindRecord(TConstArrayView<FCourtClueRecord> Courts, FCourtFixtureId Court)
{
	for (const FCourtClueRecord& Record : Courts)
	{
		if (Record.Court == Court)
		{
			return &Record;
		}
	}
	return nullptr;
}

int32 FObjectivesClue::CountMatches(TConstArrayView<FCourtClueRecord> Courts, const FClueTag& Clue)
{
	int32 Matches = 0;
	for (const FCourtClueRecord& Record : Courts)
	{
		Matches += ClueOf(Record) == Clue ? 1 : 0;
	}
	return Matches;
}
