#pragma once

// Generated-accessor templates for the schema table (ADR-0004 section 2; story tdc-041). Each
// generated row names a key through two pointers-to-member (the owner sub-struct of
// FStealthServerTuning, then the key), so a renamed or removed field is a compile error in
// StealthTuningSchema.generated.inl. Never offsetof: USTRUCTs are not standard-layout.
// Private to StealthFoundation; included only by StealthTuningSchema.cpp before the table.

#include "CoreMinimal.h"
#include "Server/StealthServerTuning.h"

#include <cmath>
#include <type_traits>

namespace StealthTuningAccess
{
	/** Widens a key value to double exactly (bool 0/1, enum by its underlying value). */
	template <typename TValue>
	double ToDouble(TValue Value)
	{
		if constexpr (std::is_enum_v<TValue>)
		{
			return static_cast<double>(static_cast<std::underlying_type_t<TValue>>(Value));
		}
		else
		{
			return static_cast<double>(Value);
		}
	}

	/** Narrows a double to the key's type (bool: non-zero; integral and enum: rounded to nearest). */
	template <typename TValue>
	TValue FromDouble(double Value)
	{
		if constexpr (std::is_same_v<TValue, bool>)
		{
			return Value != 0.0;
		}
		else if constexpr (std::is_enum_v<TValue>)
		{
			return static_cast<TValue>(static_cast<std::underlying_type_t<TValue>>(std::llround(Value)));
		}
		else if constexpr (std::is_integral_v<TValue>)
		{
			return static_cast<TValue>(std::llround(Value));
		}
		else
		{
			return static_cast<TValue>(Value);
		}
	}

	/** Generated getter: (Tuning.*OwnerMember).*KeyMember widened to double. */
	template <auto OwnerMember, auto KeyMember>
	double Get(const FStealthServerTuning& Tuning)
	{
		return ToDouble((Tuning.*OwnerMember).*KeyMember);
	}

	/** Generated setter: (Tuning.*OwnerMember).*KeyMember = Value narrowed to the key's type. */
	template <auto OwnerMember, auto KeyMember>
	void Set(FStealthServerTuning& Tuning, double Value)
	{
		auto& Field = (Tuning.*OwnerMember).*KeyMember;
		Field = FromDouble<std::remove_reference_t<decltype(Field)>>(Value);
	}
}
