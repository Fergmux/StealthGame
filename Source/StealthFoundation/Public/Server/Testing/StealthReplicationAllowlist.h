#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

class UClass;

/**
 * The checked-in replicated-property allowlist (ADR-0003 section 8; ADR-0008 section 4 fixes its path:
 * Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json). One JSON file with sections per class family and
 * rows {class, property, condition, audience, carrier, policyRows[], owners[]}; the Information Visibility Policy owns the
 * file, each row is co-owned by its system.
 *
 * STAND-IN scope (decision logged by Objectives story oc-011): the IVP epic's full parser (story ivp-013) and reflection audit
 * over every replicated class (story ivp-015) are not built. This header provides the shared parse and a per-class audit
 * (every replicated property a class declares has exactly one row, with the class's real lifetime condition, and no row names
 * a property the class does not replicate) so the rows that exist are already enforced; ivp-015 extends the audit to every
 * replicated class and ivp-013 adds the remaining rows. Compiled only under WITH_STEALTH_TEST_SEAMS (never Shipping).
 *
 * Usage:
 *   TArray<FReplicationAllowlistRow> Rows;
 *   FString Error;
 *   if (!StealthReplicationAllowlist::Load(Rows, Error)) { AddError(Error); }
 *   TArray<FString> Findings;
 *   StealthReplicationAllowlist::AuditClass(UObjectiveTeamViewComponent::StaticClass(), Rows, Findings);   // empty = pass
 */
struct FReplicationAllowlistRow
{
	/** The section the row sits in (a class family). */
	FString Section;
	/** The C++ class name with its prefix ("UObjectiveTeamViewComponent"). */
	FString Class;
	/** The replicated property's name. */
	FString Property;
	/** The engine lifetime condition name ("COND_None", "COND_OwnerOnly", ...). */
	FString Condition;
	/** The IVP audience (All, Team, Owner, ...). */
	FString Audience;
	/** The carrier (C-TeamView, C-Public, ...). */
	FString Carrier;
	/** The IVP R3 rows the field encodes. */
	TArray<FString> PolicyRows;
	/** The sign-off owners (the Policy and the row's system). */
	TArray<FString> Owners;
};

namespace StealthReplicationAllowlist
{
	/** The file's absolute path in this project (ADR-0008 section 4). */
	STEALTHFOUNDATION_API FString GetDefaultPath();

	/** Parses the allowlist JSON text. False with OutError for malformed JSON or a row missing a required field. */
	STEALTHFOUNDATION_API bool Parse(const FString& JsonText, TArray<FReplicationAllowlistRow>& OutRows, FString& OutError);

	/** Loads and parses GetDefaultPath(). */
	STEALTHFOUNDATION_API bool Load(TArray<FReplicationAllowlistRow>& OutRows, FString& OutError);

	/** The name of a lifetime condition value ("COND_None" for 0, "COND_OwnerOnly" for 2, "COND_<n>" for an unnamed one). */
	STEALTHFOUNDATION_API FString ConditionName(int32 Condition);

	/**
	 * The audit of one class (its own replicated properties, not its parents'): one finding per replicated property with no row
	 * or more than one row, per row whose condition differs from the class's registered lifetime condition, and per row naming
	 * a property the class does not replicate. Empty findings = pass.
	 */
	STEALTHFOUNDATION_API void AuditClass(const UClass* Class, TConstArrayView<FReplicationAllowlistRow> Rows, TArray<FString>& OutFindings);
}

#endif // WITH_STEALTH_TEST_SEAMS
