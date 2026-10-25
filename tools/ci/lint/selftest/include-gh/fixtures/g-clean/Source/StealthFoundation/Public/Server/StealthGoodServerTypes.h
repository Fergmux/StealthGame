#pragma once
#include "StealthServerOnlyGuard.h"
// UCLASS() in a comment, and BlueprintCallable too
/* UINTERFACE(MinimalAPI) in a block comment */
UCLASS(NotBlueprintType, NotBlueprintable)
class UStealthGoodClass : public UObject
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere)
	int32 Plain;
	UFUNCTION()
	void Plain2();
	const TCHAR* Text = TEXT("BlueprintCallable in a string");
};
UCLASS(MinimalAPI,
	NotBlueprintable,
	NotBlueprintType)
class UStealthGoodMultiLine : public UObject { GENERATED_BODY() };
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UStealthGoodInterface : public UInterface { GENERATED_BODY() };
UINTERFACE(meta = (DisplayName = "Shared list", CannotImplementInterfaceInBlueprint), MinimalAPI)
class UStealthGoodInterfaceSharedMeta : public UInterface { GENERATED_BODY() };
class FStealthEventLogPlain
{
public:
	int32 Count = 0;
};
