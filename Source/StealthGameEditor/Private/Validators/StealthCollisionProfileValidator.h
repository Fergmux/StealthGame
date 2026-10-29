#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"

#include "StealthCollisionProfileValidator.generated.h"

struct FBodyInstance;

/**
 * The camera-channel asset audit (ADR-0008 asset audit, ADR-0015 section 4.1; story cs-007 AC3): a kit or pawn asset whose
 * ViewBlock or Camera response differs from what its collision profile prescribes fails validation, naming the asset and
 * the response. It reads the asset as loaded (a static mesh's default body instance, a Blueprint's default-object
 * components) and never saves anything. Profiles outside StealthCollision::ProfileTable are not governed. An override in the
 * editor turns a body's preset into Custom; a Custom Pawn-type body is held to the Pawn row and any other Custom body that
 * blocks ViewBlock must block Camera.
 */
namespace StealthCollisionAudit
{
	/**
	 * The mismatches of one body instance against its profile's expectation: one line per channel ("ViewBlock: Block, the
	 * profile 'Pawn' prescribes Ignore"). Empty when the profile is ungoverned or every response matches.
	 */
	TArray<FString> CheckBody(const FBodyInstance& Body);
}

UCLASS()
class UStealthCollisionProfileValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};
