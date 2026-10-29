// Story con-030 AC1 (production/epics/concealment-bushes/story-030-concealment-log-and-metric-calculators.md; GDD CON-R15;
// ADR-0017 section 10): StealthGame.Concealment.ConcealmentLog.
//   - Commit, ExitCross, Emerge, Killed and BushStrike each append one ServerOnly row {Tick, BushId, PawnKind, Event} to the
//     Round Event Log slice through IRoundLogWriter (the core's only write path), with the player / NPC kind;
//   - BushStrike rows carry the candidate-set size before and after the reach filter (0 is valid): from Assassination's
//     resolver over the REAL core (an empty bush, one out-of-reach occupant, one in reach);
//   - Emerge rows record whether the strike killed (OnStrikeKilled in the Attack's slot), a whiff writes 0;
//   - the slice is never released (FRoundEventLog::GetReleaseClass) and the concealment log code names no UE_LOG / UE_LOGF
//     (lint rule (i); a source scan with a seeded control).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ConcealmentStrikeAdapters.h"
#include "ConcealmentTestRig.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Server/ConcealmentLog.h"
#include "Server/EventLog/StealthRoundEventLog.h"
#include "Tests/Assassination/EncounterTestRig.h"

using namespace StealthConcealmentTest;

namespace ConcealmentLogTestData
{
	/** The concealment rows of the rig's log, decoded. */
	TArray<FConcealmentLogRow> Rows(const FRig& Rig) { return ConcealmentLog::DecodeAll(Rig.Q.Log.Entries); }

	const FConcealmentLogRow* Find(const TArray<FConcealmentLogRow>& Rows, EConcealmentLogEvent Event, int32 Nth = 0)
	{
		for (const FConcealmentLogRow& Row : Rows)
		{
			if (Row.Event == Event && Nth-- == 0)
			{
				return &Row;
			}
		}
		return nullptr;
	}

	/** An Attack from Attacker whose slot runs Assassination's two hooks: the break at execution and, if bKill, the kill. */
	void SubmitAttack(FRig& Rig, APawn* Attacker, bool bKill)
	{
		const double ContactAfter = static_cast<double>(Rig.Params.Public.BushStrikeEmergeSec) + Rig.Params.StrikeContactSec;
		Rig.Resolver.OnOtherCommand = [&Rig, ContactAfter, bKill](const FQueueItem& Item, FQueueKey Key)
		{
			const FCommandPayload& Cmd = Item.Payload.Get<FCommandPayload>();
			if (static_cast<ECommandKind>(Cmd.Kind) != ECommandKind::Attack)
			{
				return EResolveResult::Voided;
			}
			APawn* Pawn = Cmd.Pawn.Pawn.Get();
			Rig.Core->OnAttackExecuted(*Pawn, Rig.Now() + ContactAfter, Key, Item);
			if (bKill)
			{
				Rig.Core->OnStrikeKilled(*Pawn); // AttackResolverCore::ApplyKill, same slot, after OnActionStarted(Attack)
			}
			return EResolveResult::Executed;
		};
		FCommandPayload Cmd;
		Cmd.Pawn.Pawn = Attacker;
		Cmd.Kind = static_cast<uint8>(ECommandKind::Attack);
		Rig.Q.Queue->Submit(FMailboxId::ServerLocal(), FQueueItem::Make(EQueueItemType::PawnCommand, Rig.Q.Epoch.Current, FMailboxId::ServerLocal(), Cmd));
	}

	/** True when Text names a log macro (lint rule (i)). */
	bool NamesLogMacro(const FString& Text)
	{
		return Text.Contains(TEXT("UE_LOG"), ESearchCase::CaseSensitive) || Text.Contains(TEXT("UE_LOGF"), ESearchCase::CaseSensitive);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentLogTest, "StealthGame.Concealment.ConcealmentLog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentLogTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentLogTestData;

	// Commit and ExitCross of a player pawn; Commit and Killed of an NPC; Emerge with and without a kill.
	{
		FRig Rig(TEXT("ConcealmentLogRows"));
		AStealthPawn* Player = Rig.SpawnAtReach(30.0);
		AStealthPawn* Npc = Rig.SpawnAtReach(30.0, 40.0);
		Rig.Relations.Bind(Player, 21, 0);
		Rig.EnterAndSettle(Player);
		Rig.EnterAndSettle(Npc);
		Rig.SubmitHide(Player, ECommandKind::HideExit, 0);
		Rig.Run(Rig.Params.Public.BushExitDurSec + 2.0 * FrameSec);
		Rig.Tick();
		Rig.Core->OnPawnKilled(*Npc, FQueueKey(77, 3));

		const TArray<FConcealmentLogRow> All = Rows(Rig);
		const FConcealmentLogRow* PlayerCommit = Find(All, EConcealmentLogEvent::Commit, 0);
		const FConcealmentLogRow* NpcCommit = Find(All, EConcealmentLogEvent::Commit, 1);
		const FConcealmentLogRow* Cross = Find(All, EConcealmentLogEvent::ExitCross);
		const FConcealmentLogRow* Killed = Find(All, EConcealmentLogEvent::Killed);
		TestTrue(TEXT("Commit rows for the player and the NPC"), PlayerCommit != nullptr && NpcCommit != nullptr);
		TestTrue(TEXT("one ExitCross row and one Killed row"), Cross != nullptr && Killed != nullptr
			&& Rig.Count(EConcealmentLogEvent::ExitCross) == 1 && Rig.Count(EConcealmentLogEvent::Killed) == 1);
		if (PlayerCommit != nullptr && NpcCommit != nullptr && Cross != nullptr && Killed != nullptr)
		{
			TestTrue(TEXT("the player's rows: BushId 1, PawnKind player, its participant"), PlayerCommit->BushId == 1 && PlayerCommit->bPlayer
				&& PlayerCommit->Participant == FParticipantId(21) && Cross->bPlayer && Cross->Participant == FParticipantId(21));
			TestTrue(TEXT("the NPC's rows: PawnKind NPC, no participant"), !NpcCommit->bPlayer && !NpcCommit->Participant.IsValid() && !Killed->bPlayer);
			TestTrue(TEXT("Tick is the slot key's ServerTickIndex (the kill's key)"), Killed->Tick == 77);
			TestTrue(TEXT("rows in time order: commit before cross"), PlayerCommit->ServerTime < Cross->ServerTime);
		}
		TestTrue(TEXT("every concealment entry is the Concealment type with a known code"),
			All.Num() == Rig.Q.Log.Entries.FilterByPredicate([](const FRoundLogEntry& E) { return E.Type == ERoundLogType::Concealment; }).Num());
	}
	for (const bool bKill : { true, false })
	{
		FRig Rig(bKill ? TEXT("ConcealmentLogEmergeKill") : TEXT("ConcealmentLogEmergeWhiff"));
		AStealthPawn* Hider = Rig.SpawnAtReach(30.0);
		Rig.EnterAndSettle(Hider);
		SubmitAttack(Rig, Hider, bKill);
		Rig.Run(Rig.Params.AttackLockoutSec + 2.0 * FrameSec);
		const FConcealmentLogRow* Emerge = Find(Rows(Rig), EConcealmentLogEvent::Emerge);
		TestTrue(FString::Printf(TEXT("%s: one Emerge row with kill flag %d"), bKill ? TEXT("kill") : TEXT("whiff"), bKill ? 1 : 0),
			Emerge != nullptr && Rig.Count(EConcealmentLogEvent::Emerge) == 1 && Emerge->bKilled == bKill);
	}
	// A kill by a pawn that is not emerging changes no record (only the emerging striker's row carries it).
	{
		FRig Rig(TEXT("ConcealmentLogOutsideKill"));
		AStealthPawn* Outside = Rig.SpawnAtReach(30.0);
		Rig.Core->OnStrikeKilled(*Outside);
		TestTrue(TEXT("an Outside striker's kill writes no concealment row"), Rows(Rig).Num() == 0);
	}

	// BushStrike: Assassination's resolver over the real core; candidates before and after the reach filter.
	{
		FRig Rig(TEXT("ConcealmentLogBushStrike"));
		FCoreSpotQuery Spot;
		Spot.Core = Rig.Core.Get();
		AStealthPawn* Near = Rig.SpawnAtReach(30.0);
		AStealthPawn* Far = Rig.SpawnAtReach(30.0, 60.0);
		Rig.EnterAndSettle(Near);
		Rig.EnterAndSettle(Far);
		// The core records the strike (as the service forwards IBushSpotQuery::OnBushStrike); the counts are the resolver's.
		Spot.Core->OnBushStrike(*Near, 1, 2, 1, FQueueKey(90, 1));
		Spot.Core->OnBushStrike(*Near, 1, 0, 0, FQueueKey(91, 1));
		const TArray<FConcealmentLogRow> All = Rows(Rig);
		const FConcealmentLogRow* First = Find(All, EConcealmentLogEvent::BushStrike, 0);
		const FConcealmentLogRow* Empty = Find(All, EConcealmentLogEvent::BushStrike, 1);
		TestTrue(TEXT("BushStrike rows carry before / after the reach filter"), First != nullptr && First->CandidatesBeforeReach == 2 && First->CandidatesAfterReach == 1 && First->Tick == 90);
		TestTrue(TEXT("an empty candidate set is a valid row (0 / 0)"), Empty != nullptr && Empty->CandidatesBeforeReach == 0 && Empty->CandidatesAfterReach == 0);
	}
	{
		// The resolver side: FAttackResolverCore reports rule 2's counts and the kill to IBushSpotQuery in the Attack's slot.
		using namespace StealthEncounterTest;
		const FVector BushCenter(180.0, 0.0, 0.0);
		struct FCase
		{
			const TCHAR* Name;
			int32 InReach;
			int32 OutOfReach;
		};
		for (const FCase Case : { FCase{ TEXT("empty"), 0, 0 }, FCase{ TEXT("one out of reach"), 0, 1 }, FCase{ TEXT("one in reach, one out"), 1, 1 } })
		{
			StealthEncounterTest::FRig R(TEXT("ConcealmentLogResolver"));
			APawn* A = R.Spawn(FVector::ZeroVector, 0);
			FFakeBushes::FBush& Bush = R.AddBush(1, BushCenter, 60.0);
			for (int32 Index = 0; Index < Case.InReach; ++Index)
			{
				APawn* Occupant = R.Spawn(BushCenter, 1);
				R.Conceal(Occupant, 1);
			}
			for (int32 Index = 0; Index < Case.OutOfReach; ++Index)
			{
				APawn* Occupant = R.Spawn(BushCenter + FVector(0.0, 0.0, 400.0), 1); // |dz| beyond StrikeVerticalReachCm
				R.Conceal(Occupant, 1);
			}
			R.Attack(A, BushCenter);
			const bool bOne = R.Bushes.Strikes.Num() == 1;
			TestTrue(FString::Printf(TEXT("%s: one OnBushStrike in the slot"), Case.Name), bOne);
			if (bOne)
			{
				const FFakeBushes::FStrike& Strike = R.Bushes.Strikes[0];
				TestTrue(FString::Printf(TEXT("%s: before %d / after %d"), Case.Name, Case.InReach + Case.OutOfReach, Case.InReach),
					Strike.BushId == 1 && Strike.Attacker == A && Strike.Before == Case.InReach + Case.OutOfReach && Strike.After == Case.InReach && Strike.Key == R.Last().Key);
			}
			TestEqual(FString::Printf(TEXT("%s: OnStrikeKilled only for a kill"), Case.Name), R.Bushes.StrikeKills.Num(), Case.InReach > 0 ? 1 : 0);
			(void)Bush;
		}
		// An open-ground strike (no zone) reports nothing.
		StealthEncounterTest::FRig R(TEXT("ConcealmentLogOpenGround"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		R.Attack(A, FVector(150.0, 0.0, 0.0));
		TestEqual(TEXT("open ground: no BushStrike"), R.Bushes.Strikes.Num(), 0);
	}

	// Never released, never through a log macro.
	TestTrue(TEXT("the Concealment slice is NeverReleased"), FRoundEventLog::GetReleaseClass(ERoundLogType::Concealment) == ERoundLogReleaseClass::NeverReleased);
	TestTrue(TEXT("control: the lint sees a log macro"), NamesLogMacro(TEXT("UE_LOGF(LogStealth, Display, \"x\");")));
	const FString Root = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthGameplay/Features/Concealment"));
	for (const TCHAR* File : { TEXT("Private/ConcealmentLog.cpp"), TEXT("Private/ConcealmentMetrics.cpp"), TEXT("Public/Server/ConcealmentLog.h") })
	{
		FString Text;
		if (TestTrue(FString::Printf(TEXT("%s is readable"), File), FFileHelper::LoadFileToString(Text, *FPaths::Combine(Root, File))))
		{
			// The header's comment names the rule; strip comment lines before the scan.
			TArray<FString> Lines;
			Text.ParseIntoArrayLines(Lines);
			FString Code;
			for (const FString& Line : Lines)
			{
				const FString Trimmed = Line.TrimStart();
				if (!Trimmed.StartsWith(TEXT("//")) && !Trimmed.StartsWith(TEXT("*")) && !Trimmed.StartsWith(TEXT("/*")))
				{
					Code += Line + TEXT("\n");
				}
			}
			TestFalse(FString::Printf(TEXT("%s names no UE_LOG / UE_LOGF"), File), NamesLogMacro(Code));
		}
	}
	{
		// The core's LogRow body goes through MakeEntry and the writer only.
		FString Core;
		if (TestTrue(TEXT("the core source is readable"), FFileHelper::LoadFileToString(Core, *FPaths::Combine(Root, TEXT("Private/ConcealmentCore.cpp")))))
		{
			const int32 Start = Core.Find(TEXT("void FConcealmentCore::LogRow("));
			const int32 End = Start != INDEX_NONE ? Core.Find(TEXT("\n}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start) : INDEX_NONE;
			if (TestTrue(TEXT("LogRow's body was found"), Start != INDEX_NONE && End != INDEX_NONE))
			{
				const FString Body = Core.Mid(Start, End - Start);
				TestTrue(TEXT("LogRow appends through IRoundLogWriter, no log macro"), Body.Contains(TEXT("Deps.Log->Append(")) && !NamesLogMacro(Body));
			}
		}
	}
	return true;
}

#endif
