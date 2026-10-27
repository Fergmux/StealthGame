#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "View/CommandReplyTypes.h"

class APawn;

/**
 * The owner-visible tally an FStrikeResult carries (GDD ASN-R19, Penalty CR-2 / PEN-R3, PEN-R14; story asn-015).
 * TallyAfter is the attacker's "Wrong targets" tally after this strike (it rises by one for every WrongTarget, an NPC and
 * an informant kill alike, saturating at 255); bExiled is true only on the kill that triggered the strike-limit removal.
 * Neither is the true strike count, which never reaches a client.
 */
struct FStrikeTally
{
	uint8 TallyAfter = 0;
	bool bExiled = false;

	bool operator==(const FStrikeTally& Other) const { return TallyAfter == Other.TallyAfter && bExiled == Other.bExiled; }
};

/**
 * IStrikeTallySource: declared by Assassination (consumer-owned, ADR-0018 section 2.1 step 8), implemented by the NPC-Kill
 * Penalty epic (not built: the composition root registers a stand-in over the prototype counters), registered weakly by
 * the composition root (FEncounterServiceInit::Tally).
 *
 * The resolver asks it once for EVERY executed Attack, whatever the Result (a uniform call pattern), inside the Attack's
 * slot and after the OnPawnKilled fan-out, so Penalty's strike, tally and exile follow-ons (which run inline in that
 * fan-out) have already applied. A missing source is a fault: the result carries {0, false} and the fault sink gets one
 * DeadRegistration report, never a crash. Penalty decides bExiled itself; it is never read from Roster state.
 *
 * Usage (Penalty's implementation):
 *   virtual FStrikeTally GetTallyForResult(const APawn* Attacker, EStrikeResult Result) override
 *   {
 *       return { Saturate(WrongTargetTally(Attacker)), ExiledByThisSlot(Attacker) };
 *   }
 */
class IStrikeTallySource
{
public:
	virtual ~IStrikeTallySource() = default;

	/** The attacker's tally after the strike that produced Result (called after OnPawnKilled, in the same slot). */
	virtual FStrikeTally GetTallyForResult(const APawn* Attacker, EStrikeResult Result) = 0;

	/**
	 * The same query with the Attack's ClientCommandSeq (story nkp-003: Penalty stores it as the owner re-push's LastResultSeq,
	 * PEN-R3). The resolver calls this one; the default forwards to GetTallyForResult, so a source that needs no sequence
	 * overrides only that.
	 */
	virtual FStrikeTally GetTallyForStrike(const APawn* Attacker, EStrikeResult Result, const FClientCommandSeq& Seq) { return GetTallyForResult(Attacker, Result); }
};
