// Concealment story con-022 AC1 (ADR-0017 section 6): the three gating flags (bCastHiddenShadow, bAffectIndirectLighting-
// WhileHidden, bRayTracingFarField) are false on every primitive of the class defaults of the pawn the game spawns
// (UStealthSettings::CharacterClass, the sample-animated Blueprint) and of AStealthPawn itself, so a primitive hidden by
// SetVisibility(false, true) keeps no render proxy. Negative control: a primitive with one flag set is reported.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "StealthCharacter.h"
#include "StealthSettings.h"

namespace PawnRenderFlagParityLocal
{
	bool FlagsClear(const UPrimitiveComponent* Primitive)
	{
		return !Primitive->bCastHiddenShadow && !Primitive->bAffectIndirectLightingWhileHidden && !Primitive->bRayTracingFarField;
	}

	/** Every primitive of a class's defaults (native subobjects and Blueprint-added templates). */
	void Gather(UClass* Class, TArray<const UPrimitiveComponent*>& Out)
	{
		const AActor* Defaults = Class != nullptr ? Cast<AActor>(Class->GetDefaultObject()) : nullptr;
		if (Defaults == nullptr)
		{
			return;
		}
		TInlineComponentArray<UPrimitiveComponent*> Native;
		Defaults->GetComponents(Native);
		Out.Append(Native);
		for (UClass* Walk = Class; Walk != nullptr; Walk = Walk->GetSuperClass())
		{
			const UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(Walk);
			if (Generated != nullptr && Generated->SimpleConstructionScript != nullptr)
			{
				for (const USCS_Node* Node : Generated->SimpleConstructionScript->GetAllNodes())
				{
					if (const UPrimitiveComponent* Template = Node != nullptr ? Cast<UPrimitiveComponent>(Node->ComponentTemplate) : nullptr)
					{
						Out.Add(Template);
					}
				}
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPawnRenderFlagParityTest, "StealthGame.Pawn.RenderFlagParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPawnRenderFlagParityTest::RunTest(const FString& Parameters)
{
	using namespace PawnRenderFlagParityLocal;
	// Arrange: the native pawn and the game's configured character class.
	UClass* Configured = UStealthSettings::Get()->CharacterClass.LoadSynchronous();
	TestNotNull(TEXT("the configured character class loads"), Configured);
	TArray<UClass*> Classes = { AStealthCharacter::StaticClass() };
	if (Configured != nullptr)
	{
		Classes.Add(Configured);
	}
	// Act + assert.
	for (UClass* Class : Classes)
	{
		TArray<const UPrimitiveComponent*> Primitives;
		Gather(Class, Primitives);
		TestTrue(FString::Printf(TEXT("%s has primitives (the mesh and the capsule)"), *Class->GetName()), Primitives.Num() >= 2);
		for (const UPrimitiveComponent* Primitive : Primitives)
		{
			TestTrue(FString::Printf(TEXT("%s.%s: the three gating flags are false"), *Class->GetName(), *Primitive->GetName()), FlagsClear(Primitive));
		}
	}
	// Negative control: one flag set is detected.
	UPrimitiveComponent* Control = NewObject<UPrimitiveComponent>(GetTransientPackage(), UStaticMeshComponent::StaticClass());
	Control->bCastHiddenShadow = true;
	TestFalse(TEXT("control: bCastHiddenShadow = true is reported"), FlagsClear(Control));
	return true;
}

#endif
