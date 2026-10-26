#pragma once

#include "CoreMinimal.h"

#include "CourtFixtureId.generated.h"

/**
 * A court fixture id (ADR-0014 owns its meaning; ADR-0002 section 2a carries the raw value as FSubjectRef
 * CourtSocketId). Declared in Foundation View by story iif-001 because the pawn command interface needs a reflected
 * id type for InteractBegin and FNetCommandArgs (ADR-0011 section 2) before the Level Socket Contract epic lands; the
 * width matches FSubjectRef::CourtSocketId. Value 0 is "no fixture". An id only: never a pointer, never a role.
 */
USTRUCT()
struct STEALTHFOUNDATION_API FCourtFixtureId
{
	GENERATED_BODY()

	UPROPERTY()
	uint16 Value = 0;

	FCourtFixtureId() = default;
	explicit FCourtFixtureId(uint16 InValue) : Value(InValue) {}

	/** True for an id that names a fixture. */
	bool IsSet() const { return Value != 0; }

	bool operator==(const FCourtFixtureId& Other) const { return Value == Other.Value; }
	bool operator!=(const FCourtFixtureId& Other) const { return Value != Other.Value; }

	friend uint32 GetTypeHash(const FCourtFixtureId& Id) { return ::GetTypeHash(Id.Value); }
};
