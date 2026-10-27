// Stories asn-005 to asn-011: the resolver's frozen parameters. See Server/EncounterTypes.h.

#include "Server/EncounterTypes.h"

#include "Server/StealthAssassinationInvariants.h"
#include "Server/StealthServerTuning.h"

FEncounterParams FEncounterParams::FromTuning(const FStealthServerTuning& Values)
{
	const FAssassinationInvariantInputs Foreign = FAssassinationInvariantInputs::FromValues(Values);
	FEncounterParams Out;
	Out.Public = Values.Assassination_Public;
	Out.Server = Values.Assassination_Server;
	Out.CapsuleRadiusCm = Foreign.CapsuleRadiusCm;
	Out.BushStrikeEmergeSec = Foreign.BushStrikeEmergeSec;
	Out.PawnRefAgreementCm = Foreign.PawnRefAgreementCm;
	Out.ReleaseDebounceSec = StealthEncounterPendingKeys::ReleaseDebounceSec;
	return Out;
}

FEncounterParams FEncounterParams::Defaults()
{
	return FromTuning(FStealthServerTuning());
}

FStrikeReachParams FEncounterParams::ReachParams(EStrikeReachVariant Variant) const
{
	return FStrikeReachParams::FromTuning(Public, Server, Variant, BushStrikeEmergeSec, CapsuleRadiusCm);
}

double FEncounterParams::ContactSec(EStrikeReachVariant Variant) const
{
	return static_cast<double>(Public.StrikeContactSec) + (Variant == EStrikeReachVariant::BushStrike ? BushStrikeEmergeSec : 0.0);
}
