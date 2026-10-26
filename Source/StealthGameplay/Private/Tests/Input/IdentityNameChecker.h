#pragma once

// Shared reflection checks of the pawn command interface tests (stories iif-001 and iif-006).
//
// "Matching" (pinned by story iif-006): a case-insensitive substring match of a field name, parameter name or type
// name against { Controller, PlayerState, Team, IsAI, IsPlayer }, plus any object-typed property whose class derives
// from AController or APlayerState. "ControlEpoch" does not match ("Control" is not "Controller").

#include "CoreMinimal.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "UObject/UnrealType.h"

namespace StealthIdentityCheck
{
	/** The identity-shaped name fragments (ADR-0011 section 1, IIF R2). */
	inline const TArray<FString>& ForbiddenFragments()
	{
		static const TArray<FString> Fragments = { TEXT("Controller"), TEXT("PlayerState"), TEXT("Team"), TEXT("IsAI"), TEXT("IsPlayer") };
		return Fragments;
	}

	/** True when Name contains a forbidden fragment (case-insensitive). */
	inline bool NameMatchesIdentity(const FString& Name)
	{
		for (const FString& Fragment : ForbiddenFragments())
		{
			if (Name.Contains(Fragment, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	/** One reflected name and its C++ type, as the checker sees it. */
	struct FNamedField
	{
		FString Owner;
		FString Name;
		FString Type;
		/** The class of an object-typed field (null otherwise). */
		const UClass* ObjectClass = nullptr;
	};

	/** The class an object-like property points at (object, weak, soft, lazy, interface), else null. */
	inline const UClass* ObjectClassOf(const FProperty* Property)
	{
		if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
		{
			return Object->PropertyClass;
		}
		if (const FInterfaceProperty* Interface = CastField<FInterfaceProperty>(Property))
		{
			return Interface->InterfaceClass;
		}
		return nullptr;
	}

	/** Every property of Struct, recursing into nested USTRUCT properties. */
	inline void CollectFields(const UStruct* Struct, TArray<FNamedField>& Out, const FString& Prefix = FString())
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FProperty* Property = *It;
			const FString Owner = Prefix.IsEmpty() ? Struct->GetName() : Prefix;
			Out.Add({ Owner, Property->GetName(), Property->GetCPPType(), ObjectClassOf(Property) });
			if (const FStructProperty* Nested = CastField<FStructProperty>(Property))
			{
				CollectFields(Nested->Struct, Out, Owner + TEXT(".") + Property->GetName());
			}
		}
	}

	/** The violations among Fields: a forbidden fragment in the name or type, or a controller/PlayerState class. */
	inline TArray<FString> FindViolations(const TArray<FNamedField>& Fields)
	{
		TArray<FString> Violations;
		for (const FNamedField& Field : Fields)
		{
			const bool bIdentityClass = Field.ObjectClass
				&& (Field.ObjectClass->IsChildOf(AController::StaticClass()) || Field.ObjectClass->IsChildOf(APlayerState::StaticClass()));
			if (NameMatchesIdentity(Field.Name) || NameMatchesIdentity(Field.Type) || bIdentityClass)
			{
				Violations.Add(FString::Printf(TEXT("%s.%s (%s)"), *Field.Owner, *Field.Name, *Field.Type));
			}
		}
		return Violations;
	}

	/** The violations of a struct's fields (recursive). */
	inline TArray<FString> FindStructViolations(const UStruct* Struct)
	{
		TArray<FNamedField> Fields;
		CollectFields(Struct, Fields);
		return FindViolations(Fields);
	}

	/** True when any field of Struct (recursive) is object-typed (a UObject pointer, weak, soft or interface). */
	inline TArray<FString> FindObjectFields(const UStruct* Struct)
	{
		TArray<FNamedField> Fields;
		CollectFields(Struct, Fields);
		TArray<FString> Out;
		for (const FNamedField& Field : Fields)
		{
			if (Field.ObjectClass)
			{
				Out.Add(FString::Printf(TEXT("%s.%s"), *Field.Owner, *Field.Name));
			}
		}
		return Out;
	}
}
