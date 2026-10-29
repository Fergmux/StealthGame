// Story cs-007 AC3: the camera-channel asset audit. See StealthCollisionProfileValidator.h.

#include "Validators/StealthCollisionProfileValidator.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/BodySetup.h"
#include "View/Core/StealthCollisionChannels.h"

#define LOCTEXT_NAMESPACE "StealthCollisionProfileValidator"

namespace StealthCollisionAuditLocal
{
	const TCHAR* ResponseName(ECollisionResponse Response)
	{
		switch (Response)
		{
		case ECR_Ignore: return TEXT("Ignore");
		case ECR_Overlap: return TEXT("Overlap");
		default: return TEXT("Block");
		}
	}

	/** Every primitive template of an actor class: the default object's components and the Blueprint's construction script. */
	void CollectBodies(const UBlueprint& Blueprint, TArray<TPair<FString, const FBodyInstance*>>& Out)
	{
		const UClass* Class = Blueprint.GeneratedClass;
		const AActor* Default = Class != nullptr ? Cast<AActor>(Class->GetDefaultObject()) : nullptr;
		if (Default != nullptr)
		{
			TInlineComponentArray<UPrimitiveComponent*> Primitives(Default);
			for (const UPrimitiveComponent* Primitive : Primitives)
			{
				Out.Add({ Primitive->GetName(), &Primitive->BodyInstance });
			}
		}
		if (const USimpleConstructionScript* Script = Blueprint.SimpleConstructionScript)
		{
			for (const USCS_Node* Node : Script->GetAllNodes())
			{
				if (const UPrimitiveComponent* Primitive = Node != nullptr ? Cast<UPrimitiveComponent>(Node->ComponentTemplate) : nullptr)
				{
					Out.Add({ Node->GetVariableName().ToString(), &Primitive->BodyInstance });
				}
			}
		}
	}
}

TArray<FString> StealthCollisionAudit::CheckBody(const FBodyInstance& Body)
{
	using namespace StealthCollisionAuditLocal;
	TArray<FString> Out;
	const FName Profile = Body.GetCollisionProfileName();
	const ECollisionResponse ViewBlock = Body.GetResponseToChannel(StealthCollision::ViewBlock);
	const ECollisionResponse Camera = Body.GetResponseToChannel(ECC_Camera);
	if (Profile == UCollisionProfile::CustomCollisionProfileName)
	{
		// A response overridden on an asset turns its preset into Custom (the named profile cannot drift on load). The rule
		// for Custom (story cs-007 QA note): a Pawn-type body is held to the Pawn row; any other body that blocks ViewBlock
		// must block Camera, or the lens could pass through what the eye cannot see through.
		if (Body.GetObjectType() == ECC_Pawn)
		{
			const StealthCollision::FProfileExpectation* PawnRow = StealthCollision::FindExpectation(UCollisionProfile::Pawn_ProfileName);
			if (PawnRow != nullptr && ViewBlock != PawnRow->ViewBlock)
			{
				Out.Add(FString::Printf(TEXT("ViewBlock: %s, a Custom Pawn body must match the profile 'Pawn' (%s)"), ResponseName(ViewBlock), ResponseName(PawnRow->ViewBlock)));
			}
			if (PawnRow != nullptr && Camera != PawnRow->Camera)
			{
				Out.Add(FString::Printf(TEXT("Camera: %s, a Custom Pawn body must match the profile 'Pawn' (%s)"), ResponseName(Camera), ResponseName(PawnRow->Camera)));
			}
		}
		else if (ViewBlock == ECR_Block && Camera != ECR_Block)
		{
			Out.Add(FString::Printf(TEXT("Camera: %s on a Custom body that blocks ViewBlock (a sight blocker must block the camera boom)"), ResponseName(Camera)));
		}
		return Out;
	}
	const StealthCollision::FProfileExpectation* Expected = StealthCollision::FindExpectation(Profile);
	if (Expected == nullptr)
	{
		return Out;
	}
	if (ViewBlock != Expected->ViewBlock)
	{
		Out.Add(FString::Printf(TEXT("ViewBlock: %s, the profile '%s' prescribes %s"), ResponseName(ViewBlock), *Profile.ToString(), ResponseName(Expected->ViewBlock)));
	}
	if (Camera != Expected->Camera)
	{
		Out.Add(FString::Printf(TEXT("Camera: %s, the profile '%s' prescribes %s"), ResponseName(Camera), *Profile.ToString(), ResponseName(Expected->Camera)));
	}
	return Out;
}

bool UStealthCollisionProfileValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const
{
	if (const UBlueprint* Blueprint = Cast<UBlueprint>(InObject))
	{
		return Blueprint->GeneratedClass != nullptr && Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass());
	}
	return Cast<UStaticMesh>(InObject) != nullptr;
}

EDataValidationResult UStealthCollisionProfileValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
	TArray<TPair<FString, const FBodyInstance*>> Bodies;
	if (const UStaticMesh* Mesh = Cast<UStaticMesh>(InAsset))
	{
		if (const UBodySetup* Setup = Mesh->GetBodySetup())
		{
			Bodies.Add({ TEXT("default body"), &Setup->DefaultInstance });
		}
	}
	else if (const UBlueprint* Blueprint = Cast<UBlueprint>(InAsset))
	{
		StealthCollisionAuditLocal::CollectBodies(*Blueprint, Bodies);
	}
	bool bFailed = false;
	for (const TPair<FString, const FBodyInstance*>& Body : Bodies)
	{
		for (const FString& Mismatch : StealthCollisionAudit::CheckBody(*Body.Value))
		{
			bFailed = true;
			AssetFails(InAsset, FText::Format(LOCTEXT("Mismatch", "{0} ({1}): {2}"), FText::FromString(InAsset->GetPathName()), FText::FromString(Body.Key), FText::FromString(Mismatch)));
		}
	}
	if (!bFailed)
	{
		AssetPasses(InAsset);
	}
	return bFailed ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}

#undef LOCTEXT_NAMESPACE
