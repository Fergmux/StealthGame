#include "View/Core/StealthLocomotionModel.h"

EStealthCopingClass StealthLocomotion::ClassifyCopingHeight(float HeightCm, const FStealthTraversalHeights& Heights)
{
	if (HeightCm <= Heights.MinHeightCm)
	{
		return EStealthCopingClass::Step;
	}
	if (FMath::Abs(HeightCm - Heights.VaultHeightCm) <= Heights.ToleranceCm)
	{
		return EStealthCopingClass::Vault;
	}
	if (FMath::Abs(HeightCm - Heights.ClimbHeightCm) <= Heights.ToleranceCm)
	{
		return EStealthCopingClass::Climb;
	}
	return EStealthCopingClass::Block;
}
