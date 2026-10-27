// Story asn-024: the owner result plate settings. See View/OwnResultPlateSettings.h.

#include "View/OwnResultPlateSettings.h"

#include "View/OwnResultMatcher.h"

UOwnResultPlateSettings::UOwnResultPlateSettings()
{
	CategoryName = TEXT("Game");
}

const UOwnResultPlateSettings* UOwnResultPlateSettings::Get()
{
	return GetDefault<UOwnResultPlateSettings>();
}

FOwnResultMatcherParams UOwnResultPlateSettings::MatcherParams() const
{
	FOwnResultMatcherParams Out;
	Out.MaxHoldBackSec = HudResultPlateMaxHoldBackSec;
	return Out;
}

FOwnResultPlateTimelineParams UOwnResultPlateSettings::TimelineParams() const
{
	FOwnResultPlateTimelineParams Out;
	Out.InSec = HudResultPlateInSec;
	Out.HoldSec = HudResultPlateHoldSec;
	Out.ReadHoldMult = HudReadHoldMult;
	Out.OutSec = HudResultPlateOutSec;
	return Out;
}
