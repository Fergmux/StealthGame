// Objectives Chain story oc-010 (production/epics/objectives-chain/story-010-team-view-struct-and-builder.md): the fixed-width
// serialiser of FObjectiveTeamView (ADR-0020 section 4 width budget). SerializeBits only: FArchive::SerializeInt would write 32
// bits on the base archive (ADR-0018's lesson). See View/ObjectiveTeamView.h for the layout.

#include "View/ObjectiveTeamView.h"

namespace ObjectiveTeamViewLocal
{
	template <typename T>
	void SerializeField(FArchive& Ar, T& Value, int32 Bits)
	{
		uint32 Wire = static_cast<uint32>(Value);
		Ar.SerializeBits(&Wire, Bits);
		if (Ar.IsLoading())
		{
			Value = static_cast<T>(Wire & ((Bits >= 32) ? 0xFFFFFFFFu : ((1u << Bits) - 1u)));
		}
	}
}

bool FObjectiveTeamView::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	using namespace ObjectiveTeamViewLocal;
	Ar.SerializeBits(&RoundEpoch, 32);
	Ar.SerializeBits(&ChainSerial, 16);

	uint8 Stage = FMath::Min<uint8>(static_cast<uint8>(StageIndex), 3);
	SerializeField(Ar, Stage, 2);
	uint8 Rival = FMath::Min<uint8>(RivalPrelimCount, 3);
	SerializeField(Ar, Rival, 2);
	uint8 Count = static_cast<uint8>(FMath::Min(LiveSites.Num(), MaxLiveSites));
	SerializeField(Ar, Count, 2);
	Ar.SerializeBits(&CompletedDefMask, 16);
	if (Ar.IsLoading())
	{
		StageIndex = static_cast<EObjectiveTeamStage>(Stage);
		RivalPrelimCount = Rival;
		LiveSites.SetNum(Count);
	}

	for (int32 Index = 0; Index < Count; ++Index)
	{
		FObjectiveLiveSite& Site = LiveSites[Index];
		Ar.SerializeBits(&Site.Court.Value, 16);
		uint8 Def = Site.DefIndex < UObjectiveChainData::MaxDefinitions ? Site.DefIndex : NoDefWire;
		SerializeField(Ar, Def, 5);
		SerializeField(Ar, Site.Clue.AreaType, 8);
		SerializeField(Ar, Site.Clue.FeatureKind, 8);
		if (Ar.IsLoading())
		{
			Site.DefIndex = Def == NoDefWire ? 0xFF : Def;
		}
	}
	bOutSuccess = true;
	return true;
}
