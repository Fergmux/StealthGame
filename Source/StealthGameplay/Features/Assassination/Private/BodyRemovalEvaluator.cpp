// Story asn-020: VA-ASN-2's cut-at-occlusion removal rule. See BodyRemovalEvaluator.h.

#include "BodyRemovalEvaluator.h"

const TCHAR* LexBodyRemovalDecision(EBodyRemovalDecision Decision)
{
	switch (Decision)
	{
	case EBodyRemovalDecision::HideOccluded: return TEXT("HideOccluded");
	case EBodyRemovalDecision::HideOutOfFrustum: return TEXT("HideOutOfFrustum");
	case EBodyRemovalDecision::CutAtGrace: return TEXT("CutAtGrace");
	case EBodyRemovalDecision::NeverShow: return TEXT("NeverShow");
	case EBodyRemovalDecision::AlreadyHidden: return TEXT("AlreadyHidden");
	default: return TEXT("Keep");
	}
}

double FBodyRemovalEvaluator::WindowOpensAt(const FBodyRemovalInput& Input)
{
	return Input.DeathServerTime + Input.DeathFallSec + Input.BodyLingerSec;
}

double FBodyRemovalEvaluator::GraceEndsAt(const FBodyRemovalInput& Input)
{
	return WindowOpensAt(Input) + Input.BodyRemovalGraceSec;
}

bool FBodyRemovalEvaluator::Hides(EBodyRemovalDecision Decision)
{
	return Decision == EBodyRemovalDecision::HideOccluded || Decision == EBodyRemovalDecision::HideOutOfFrustum
		|| Decision == EBodyRemovalDecision::CutAtGrace || Decision == EBodyRemovalDecision::NeverShow;
}

EBodyRemovalDecision FBodyRemovalEvaluator::Evaluate(const FBodyRemovalInput& Input)
{
	constexpr double DueEpsilon = 0.001;
	if (Input.bAlreadyHidden)
	{
		return EBodyRemovalDecision::AlreadyHidden;
	}
	if (Input.DeathServerTime < 0.0)
	{
		return EBodyRemovalDecision::Keep;
	}
	if (Input.bFirstSeenAfterLinger)
	{
		return EBodyRemovalDecision::NeverShow;
	}
	if (Input.Now + DueEpsilon < WindowOpensAt(Input))
	{
		return EBodyRemovalDecision::Keep;
	}
	if (!Input.bInFrustum)
	{
		return EBodyRemovalDecision::HideOutOfFrustum;
	}
	if (Input.bFullyOccluded)
	{
		return EBodyRemovalDecision::HideOccluded;
	}
	return Input.Now + DueEpsilon >= GraceEndsAt(Input) ? EBodyRemovalDecision::CutAtGrace : EBodyRemovalDecision::Keep;
}
