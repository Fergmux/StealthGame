#pragma once
#include "StealthServerOnlyGuard.h"
UCLASS()
class UStealthNoSpecifiers : public UObject { GENERATED_BODY() };
UCLASS(NotBlueprintType)
class UStealthOnlyOneSpecifier : public UObject { GENERATED_BODY() };
UCLASS(NotBlueprintable)
class UStealthOtherOneSpecifier : public UObject { GENERATED_BODY() };
UINTERFACE(MinimalAPI)
class UStealthBadInterface : public UInterface { GENERATED_BODY() };
UINTERFACE(MinimalAPI, meta = (DisplayName = "x"))
class UStealthBadInterfaceWithMeta : public UInterface { GENERATED_BODY() };
UCLASS(NotBlueprintType, NotBlueprintable)
class UStealthGoodClassWithBadMembers : public UObject
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly)
	int32 ReadOnly;
	UPROPERTY(BlueprintReadWrite, Category = "X")
	int32 ReadWrite;
	UFUNCTION(BlueprintCallable)
	void Callable();
	UFUNCTION(BlueprintPure)
	int32 Pure() const;
	UPROPERTY(BlueprintAssignable)
	int32 Assignable;
	UPROPERTY(VisibleAnywhere,
		BlueprintReadOnly)
	int32 SpansLines;
};
