#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthQueuePayloads.h"
#include "UObject/Interface.h"
#include "View/CommandReplyTypes.h"

#include "CommandPredicate.generated.h"

class AStealthPawn;

/** A feature's answer to "may this pawn begin this action now" (ADR-0011 section 6). */
enum class EPredicateVerdict : uint8
{
	/** Execute normally. */
	Allow,
	/** Execute exactly like Allow (same pose, timer, events); only the completion changes nothing. */
	AllowNoEffect,
	/** Void the command with Code. */
	Reject,
};

/** A predicate verdict and, for Reject, the owner-visible code (ADR-0011 Key Interfaces). */
struct FPredicateResult
{
	EPredicateVerdict Verdict = EPredicateVerdict::Reject;
	ECommandRejectCode Code = ECommandRejectCode::NotNow;

	static FPredicateResult Allow() { return { EPredicateVerdict::Allow, ECommandRejectCode::NotNow }; }
	static FPredicateResult AllowNoEffect() { return { EPredicateVerdict::AllowNoEffect, ECommandRejectCode::NotNow }; }
	static FPredicateResult Reject(ECommandRejectCode InCode) { return { EPredicateVerdict::Reject, InCode }; }

	bool operator==(const FPredicateResult& Other) const { return Verdict == Other.Verdict && (Verdict != EPredicateVerdict::Reject || Code == Other.Code); }
	bool operator!=(const FPredicateResult& Other) const { return !(*this == Other); }
};

/**
 * A consumer-owned interface in Core (ADR-0011 section 6; architecture.md TD concern 1): Assassination (Prepare,
 * Attack), Objectives (Interact) and Concealment (Hide) implement it and the composition root registers them per
 * EActionType with UActionHub::RegisterPredicate, weakly. The IIF depends on this interface, the features depend on
 * the IIF, so the command interface ships before its feature owners exist (story iif-003).
 *
 * CanBegin must be const and side-effect-free: the AI dry run (QueryCanBegin) calls it outside a slot. The choice
 * between AllowNoEffect and Reject may depend only on the site and the legality class.
 *
 * A UINTERFACE so the hub can hold TWeakInterfacePtr registrations (ADR-0002 E7); UHT gives the I-class a non-pure
 * _getUObject, so a plain C++ test stub can implement it too.
 */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UCommandPredicate : public UInterface
{
	GENERATED_BODY()
};

class STEALTHGAMEPLAY_API ICommandPredicate
{
	GENERATED_BODY()

public:
	/** The verdict for Pawn beginning the payload's command now. Must be side-effect-free. */
	virtual FPredicateResult CanBegin(const AStealthPawn& Pawn, const FCommandPayload& Cmd) const = 0;
};
