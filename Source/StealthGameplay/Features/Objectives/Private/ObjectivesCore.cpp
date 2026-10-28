// Objectives Chain stories oc-003 (validation and binding), oc-004 (site state and progress), oc-006 (claims), oc-008 (the
// completion gate and stage effects), oc-009 (the final's win declaration) and oc-010 (the team view builder):
// production/epics/objectives-chain/.
// See ObjectivesCore.h.

#include "ObjectivesCore.h"

#include "GameFramework/Pawn.h"
#include "Server/StealthFaultSink.h"

namespace ObjectivesCoreLocal
{
	const FName FaultSource(TEXT("Objectives"));

	bool IsBlank(const FText& Text)
	{
		return Text.ToString().TrimStartAndEnd().IsEmpty();
	}

	EObjectiveSiteRole RoleOf(EObjectiveStage Stage)
	{
		switch (Stage)
		{
		case EObjectiveStage::Prelim1: return EObjectiveSiteRole::P1;
		case EObjectiveStage::Prelim2: return EObjectiveSiteRole::P2;
		default: return EObjectiveSiteRole::Final;
		}
	}

	EObjectiveTeamStage TeamStageOf(EObjectiveStage Stage)
	{
		switch (Stage)
		{
		case EObjectiveStage::Prelim1: return EObjectiveTeamStage::Stage1;
		case EObjectiveStage::Prelim2: return EObjectiveTeamStage::Stage2;
		default: return EObjectiveTeamStage::Final;
		}
	}

	EObjectiveStage NextStage(EObjectiveStage Stage)
	{
		return Stage == EObjectiveStage::Prelim1 ? EObjectiveStage::Prelim2 : EObjectiveStage::Final;
	}
}

void FObjectivesCore::SetTuning(const FObjectivesTuning_Public& InPublic, const FObjectivesTuning_Server& InServer)
{
	PublicTuning = InPublic;
	ServerTuning = InServer;
}

// --- Validation and binding (story oc-003) ----------------------------------------------------------------------------

FChainValidationResult FObjectivesCore::ValidateChain(const UObjectiveChainData* Chain, const FObjectivesTuning_Server& Tuning)
{
	using namespace ObjectivesCoreLocal;
	if (Chain == nullptr)
	{
		return FChainValidationResult::Fail(EChainValidationClause::NoChain);
	}
	// Rule 2: ObjectivesPerStageCount must be 1 until LSC Selection draws several courts per stage (OC-3, GDD OQ2).
	if (Tuning.ObjectivesPerStageCount != 1)
	{
		return FChainValidationResult::Fail(EChainValidationClause::PerStageCount);
	}
	const TArray<FObjectiveDef>& Defs = Chain->Definitions;
	if (Defs.Num() > UObjectiveChainData::MaxDefinitions)
	{
		return FChainValidationResult::Fail(EChainValidationClause::TooManyRows);
	}
	TSet<FName> Ids;
	int32 Prelims[TeamCount][2] = { { 0, 0 }, { 0, 0 } };
	int32 Finals = 0;
	for (int32 Row = 0; Row < Defs.Num(); ++Row)
	{
		const FObjectiveDef& Def = Defs[Row];
		bool bDuplicate = false;
		Ids.Add(Def.ObjectiveId, &bDuplicate);
		if (bDuplicate || Def.ObjectiveId.IsNone())
		{
			return FChainValidationResult::Fail(EChainValidationClause::DuplicateId, Row);
		}
		if (Def.SiteRole != RoleOf(Def.StageKind))
		{
			return FChainValidationResult::Fail(EChainValidationClause::RoleStageMismatch, Row);
		}
		if (IsBlank(Def.CardTitle))
		{
			return FChainValidationResult::Fail(EChainValidationClause::EmptyCardTitle, Row);
		}
		if (IsBlank(Def.CardLine))
		{
			return FChainValidationResult::Fail(EChainValidationClause::EmptyCardLine, Row);
		}
		// Rule 3: one tend per site in MVP (above 1 needs a GDD rule for partial progress).
		if (Def.CompletionsRequiredCount != 1)
		{
			return FChainValidationResult::Fail(EChainValidationClause::CompletionsRequired, Row);
		}
		// Rule 4 (bOverrideInteractSec) is moot since 2026-10-25: FObjectiveDef has no per-row hold time. Nothing to check.
		if (Def.StageKind == EObjectiveStage::Final)
		{
			if (Def.TeamEligibility != EObjectiveEligibility::Both)
			{
				return FChainValidationResult::Fail(EChainValidationClause::FinalRow, Row);
			}
			++Finals;
			continue;
		}
		if (Def.TeamEligibility == EObjectiveEligibility::Both)
		{
			return FChainValidationResult::Fail(EChainValidationClause::PrelimEligibility, Row);
		}
		const int32 Team = Def.TeamEligibility == EObjectiveEligibility::Team0 ? 0 : 1;
		++Prelims[Team][Def.StageKind == EObjectiveStage::Prelim1 ? 0 : 1];
	}
	for (int32 Team = 0; Team < TeamCount; ++Team)
	{
		if (Prelims[Team][0] != Tuning.ObjectivesPerStageCount || Prelims[Team][1] != Tuning.ObjectivesPerStageCount)
		{
			return FChainValidationResult::Fail(EChainValidationClause::TeamStageRows);
		}
	}
	if (Finals != 1)
	{
		return FChainValidationResult::Fail(EChainValidationClause::FinalRow);
	}
	return FChainValidationResult();
}

FChainValidationResult FObjectivesCore::Bind(const UObjectiveChainData* Chain, const IObjectivesAssignmentSource& Source)
{
	const FChainValidationResult Valid = ValidateChain(Chain, ServerTuning);
	if (!Valid.IsValid())
	{
		return Valid;
	}
	FSiteAssignment Assignment;
	if (!Source.GetSiteAssignment_Server(Assignment))
	{
		return FChainValidationResult::Fail(EChainValidationClause::AssignmentIncomplete);
	}
	// Every role needs a court, and no court holds two roles (LSC guarantees it; a hole or a reuse fails, never binds).
	const FCourtFixtureId Sites[] = { Assignment.P1[0], Assignment.P1[1], Assignment.P2[0], Assignment.P2[1], Assignment.Final };
	TSet<FCourtFixtureId> Seen;
	for (const FCourtFixtureId& Site : Sites)
	{
		bool bReused = false;
		Seen.Add(Site, &bReused);
		if (!Site.IsSet() || bReused)
		{
			return FChainValidationResult::Fail(EChainValidationClause::AssignmentIncomplete);
		}
	}

	TArray<FChainBinding, TInlineAllocator<UObjectiveChainData::MaxDefinitions>> NewBindings;
	for (const FObjectiveDef& Def : Chain->Definitions)
	{
		FChainBinding& Binding = NewBindings.AddDefaulted_GetRef();
		Binding.Stage = Def.StageKind;
		Binding.Eligibility = Def.TeamEligibility;
		Binding.ObjectiveId = Def.ObjectiveId;
		const int32 Team = Def.TeamEligibility == EObjectiveEligibility::Team1 ? 1 : 0;
		switch (Def.SiteRole)
		{
		case EObjectiveSiteRole::P1: Binding.Court = Assignment.P1[Team]; break;
		case EObjectiveSiteRole::P2: Binding.Court = Assignment.P2[Team]; break;
		default: Binding.Court = Assignment.Final; break; // bound once, for both teams
		}
	}
	Reset();
	Bindings = MoveTemp(NewBindings);
	ChainSerial = Chain->ChainSerial;
	bBound = true;
	return FChainValidationResult();
}

// --- Active entry and progress (story oc-004) ---------------------------------------------------------------------

bool FObjectivesCore::HandleActiveEntry(uint32 InRoundEpoch, double InActivePhaseStart)
{
	if (!bBound || (bActive && RoundEpoch == InRoundEpoch))
	{
		return false;
	}
	RoundEpoch = InRoundEpoch;
	ActivePhaseStart = InActivePhaseStart;
	bActive = true;
	bEnded = false;
	for (int32 Team = 0; Team < TeamCount; ++Team)
	{
		for (int32 Def = 0; Def < Bindings.Num(); ++Def)
		{
			SiteStates[Team][Def] = EObjectiveSiteState::Locked;
		}
		CompletedDefMask[Team] = 0;
		SetStageLive(Team, EObjectiveStage::Prelim1);
		StageIndex[Team] = EObjectiveTeamStage::Stage1;
	}
	for (int32 Team = 0; Team < TeamCount; ++Team)
	{
		PushView(Team);
	}
	return true;
}

void FObjectivesCore::Reset()
{
	Bindings.Reset();
	for (int32 Team = 0; Team < TeamCount; ++Team)
	{
		for (int32 Def = 0; Def < UObjectiveChainData::MaxDefinitions; ++Def)
		{
			SiteStates[Team][Def] = EObjectiveSiteState::Locked;
		}
		StageIndex[Team] = EObjectiveTeamStage::None;
		CompletedDefMask[Team] = 0;
		RivalPrelimCount[Team] = 0;
		LastPushedView[Team] = FObjectiveTeamView();
		bViewPushed[Team] = false;
	}
	ChainSerial = 0;
	PrelimOrdinal = 0;
	RoundEpoch = 0;
	ActivePhaseStart = 0.0;
	bBound = false;
	bActive = false;
	bEnded = false;
	Claims.Reset();
}

bool FObjectivesCore::IsEligible(EObjectiveEligibility Eligibility, int32 Team)
{
	return Eligibility == EObjectiveEligibility::Both || (Team == 0 && Eligibility == EObjectiveEligibility::Team0) || (Team == 1 && Eligibility == EObjectiveEligibility::Team1);
}

void FObjectivesCore::SetStageLive(int32 Team, EObjectiveStage Stage)
{
	for (int32 Def = 0; Def < Bindings.Num(); ++Def)
	{
		if (Bindings[Def].Stage == Stage && IsEligible(Bindings[Def].Eligibility, Team) && SiteStates[Team][Def] == EObjectiveSiteState::Locked)
		{
			SiteStates[Team][Def] = EObjectiveSiteState::Live;
		}
	}
}

bool FObjectivesCore::IsStageComplete(int32 Team, EObjectiveStage Stage) const
{
	for (int32 Def = 0; Def < Bindings.Num(); ++Def)
	{
		if (Bindings[Def].Stage == Stage && IsEligible(Bindings[Def].Eligibility, Team) && SiteStates[Team][Def] != EObjectiveSiteState::Completed)
		{
			return false;
		}
	}
	return true;
}

FObjectivesAppliedCompletion FObjectivesCore::ApplyCountedCompletion(int32 Team, int32 DefIndex)
{
	FObjectivesAppliedCompletion Out;
	if (!IsActive() || Team < 0 || Team >= TeamCount || !Bindings.IsValidIndex(DefIndex) || !IsEligible(Bindings[DefIndex].Eligibility, Team)
		|| SiteStates[Team][DefIndex] != EObjectiveSiteState::Live)
	{
		return Out;
	}
	const EObjectiveStage Stage = Bindings[DefIndex].Stage;
	SiteStates[Team][DefIndex] = EObjectiveSiteState::Completed;
	Out.bApplied = true;
	Out.Stage = Stage;
	if (Stage == EObjectiveStage::Final)
	{
		return Out; // the round ends by ObjectiveWin; the final is never in CompletedDefMask
	}
	CompletedDefMask[Team] |= static_cast<uint16>(1u << DefIndex);
	if (IsStageComplete(Team, Stage))
	{
		const EObjectiveStage Next = ObjectivesCoreLocal::NextStage(Stage);
		SetStageLive(Team, Next); // for Team only: the other team's rows are untouched
		StageIndex[Team] = ObjectivesCoreLocal::TeamStageOf(Next);
		Out.bUnlockedNext = true;
	}
	return Out;
}

// --- Queries ---------------------------------------------------------------------------------------------------------

EObjectiveSiteState FObjectivesCore::GetSiteState(int32 Team, int32 DefIndex) const
{
	return Team >= 0 && Team < TeamCount && Bindings.IsValidIndex(DefIndex) ? SiteStates[Team][DefIndex] : EObjectiveSiteState::Locked;
}

EObjectiveTeamStage FObjectivesCore::GetStageIndex(int32 Team) const
{
	return Team >= 0 && Team < TeamCount ? StageIndex[Team] : EObjectiveTeamStage::None;
}

uint16 FObjectivesCore::GetCompletedDefMask(int32 Team) const
{
	return Team >= 0 && Team < TeamCount ? CompletedDefMask[Team] : 0;
}

int32 FObjectivesCore::FindLiveDefinition(int32 Team, FCourtFixtureId Court) const
{
	if (Team < 0 || Team >= TeamCount || !Court.IsSet())
	{
		return INDEX_NONE;
	}
	for (int32 Def = 0; Def < Bindings.Num(); ++Def)
	{
		if (Bindings[Def].Court == Court && IsEligible(Bindings[Def].Eligibility, Team) && SiteStates[Team][Def] == EObjectiveSiteState::Live)
		{
			return Def;
		}
	}
	return INDEX_NONE;
}

void FObjectivesCore::GetLiveDefinitions(int32 Team, TArray<int32>& OutDefIndices) const
{
	OutDefIndices.Reset();
	for (int32 Def = 0; Team >= 0 && Team < TeamCount && Def < Bindings.Num(); ++Def)
	{
		if (SiteStates[Team][Def] == EObjectiveSiteState::Live)
		{
			OutDefIndices.Add(Def);
		}
	}
}

int32 FObjectivesCore::GetCompletedPreliminaryStages(int32 Team) const
{
	if (Team < 0 || Team >= TeamCount || !bBound)
	{
		return 0;
	}
	int32 Stages = 0;
	for (const EObjectiveStage Stage : { EObjectiveStage::Prelim1, EObjectiveStage::Prelim2 })
	{
		bool bAny = false;
		for (int32 Def = 0; Def < Bindings.Num(); ++Def)
		{
			bAny |= Bindings[Def].Stage == Stage && IsEligible(Bindings[Def].Eligibility, Team);
		}
		Stages += bAny && IsStageComplete(Team, Stage) ? 1 : 0;
	}
	return Stages;
}

// --- The interact-target owner (stories oc-006, oc-008, oc-009) ------------------------------------------------------

int32 FObjectivesCore::OnInteractExecuted(const APawn& Pawn, FCourtFixtureId Court, FQueueKey Key)
{
	TArray<FVector, TInlineAllocator<4>> Faces;
	if (Deps.Courts != nullptr)
	{
		Deps.Courts->GetFixtureFaces(Court, Faces);
	}
	const uint32 Epoch = Deps.PawnControl != nullptr ? Deps.PawnControl->GetControlEpoch(Pawn) : 0u;
	return Claims.Claim(Court, &Pawn, Pawn.GetActorLocation(), Faces, GetFixtureSideCount(), Epoch);
}

EObjectivesCompletion FObjectivesCore::OnInteractEnded(const APawn& Pawn, FCourtFixtureId Court, EObjectivesInteractEnd End, FQueueKey Key)
{
	uint32 RecordedEpoch = 0;
	const bool bHadClaim = Claims.Release(Court, &Pawn, &RecordedEpoch);
	if (End != EObjectivesInteractEnd::Completed || !Key.IsSet())
	{
		return EObjectivesCompletion::ClaimReleaseOnly; // outside a slot or a cancel: no gate, no log
	}

	// (b) the server-only binding lookup: the one bound/unbound branch (its own effect is only the TendCompleted entry).
	const FParticipantId Participant = Deps.Roster != nullptr ? Deps.Roster->GetParticipant(Pawn) : FParticipantId();
	const bool bBoundPawn = Participant.IsValid();

	// (a) Active and not latched (the queue already voids items after the latch; a defensive re-check).
	bool bCounts = IsActive() && Deps.Round != nullptr && Deps.Round->IsActive() && !Deps.Round->IsLatched();
	// (b) the participant is Active.
	bCounts = bCounts && bBoundPawn && Deps.Roster->GetState(Participant) == EParticipantState::Active;
	// (c) Session Connected, human-controlled for the whole hold (control epoch unchanged since the claim, no hand-over pending).
	bCounts = bCounts && Deps.Session != nullptr && Deps.Session->GetConnectionState(Participant) == EObjectivesConnectionState::Connected;
	bCounts = bCounts && bHadClaim && Deps.PawnControl != nullptr && Deps.PawnControl->GetControlEpoch(Pawn) == RecordedEpoch
		&& !Deps.PawnControl->IsHandOverPending(Pawn);
	// (d) and (e): a Live (hence not Completed) site of a row eligible for P's team.
	const int32 Team = bCounts ? Deps.Roster->GetTeam(Participant) : INDEX_NONE;
	const int32 DefIndex = bCounts ? FindLiveDefinition(Team, Court) : INDEX_NONE;
	bCounts = bCounts && DefIndex != INDEX_NONE;

	if (!bCounts)
	{
		if (bBoundPawn)
		{
			AppendTend(Participant, Court, ERoundTendResult::NoEffect, /*bLatched*/ false, Key);
		}
		return EObjectivesCompletion::NoEffect;
	}
	const EObjectivesCompletion Outcome = ApplyCountedEffects(Team, DefIndex, Participant, Key, /*bDebug*/ false);
	AppendTend(Participant, Court, Outcome == EObjectivesCompletion::NoEffect ? ERoundTendResult::NoEffect : ERoundTendResult::Counted, false, Key);
	return Outcome;
}

void FObjectivesCore::OnInteractCompleteLatched(const APawn& Pawn, FCourtFixtureId Court, FQueueKey Key)
{
	Claims.Release(Court, &Pawn);
	const FParticipantId Participant = Deps.Roster != nullptr ? Deps.Roster->GetParticipant(Pawn) : FParticipantId();
	if (Participant.IsValid())
	{
		AppendTend(Participant, Court, ERoundTendResult::NoEffect, /*bLatched*/ true, Key);
	}
}

#if !UE_BUILD_SHIPPING
bool FObjectivesCore::ApplyDebugStageCompletion(int32 Team, FQueueKey Key)
{
	TArray<int32> Live;
	GetLiveDefinitions(Team, Live);
	if (!IsActive() || Live.Num() == 0)
	{
		return false;
	}
	bool bAny = false;
	for (const int32 DefIndex : Live)
	{
		bAny |= ApplyCountedEffects(Team, DefIndex, FParticipantId(), Key, /*bDebug*/ true) != EObjectivesCompletion::NoEffect;
		if (IsEnded() || (Deps.Round != nullptr && Deps.Round->IsLatched()))
		{
			break;
		}
	}
	return bAny;
}
#endif

EObjectivesCompletion FObjectivesCore::ApplyCountedEffects(int32 Team, int32 DefIndex, FParticipantId Participant, FQueueKey Key, bool bDebug)
{
	const FObjectivesAppliedCompletion Applied = ApplyCountedCompletion(Team, DefIndex);
	if (!Applied.bApplied)
	{
		return EObjectivesCompletion::NoEffect;
	}
	if (Applied.Stage == EObjectiveStage::Final)
	{
		// The final: FinalCompleted, then the win declaration in this same slot with its key. No view write, no release.
		Append(ERoundLogType::FinalCompleted, Key, Participant, [Team, bDebug](FRoundLogEntry& Entry)
		{
			Entry.Data[0] = static_cast<uint32>(Team);
			Entry.Flags |= bDebug ? RoundLogFlags::Debug : 0;
		});
		if (Deps.Round != nullptr)
		{
			Deps.Round->DeclareObjectiveWin(static_cast<uint8>(Team), Key);
		}
		return EObjectivesCompletion::CountedFinal;
	}
	++PrelimOrdinal;
	const uint32 Ordinal = PrelimOrdinal;
	const uint16 Serial = ChainSerial;
	Append(ERoundLogType::ObjectiveStage, Key, Participant, [Team, &Applied, DefIndex, Serial, bDebug](FRoundLogEntry& Entry)
	{
		Entry.Data[0] = static_cast<uint32>(Team);
		Entry.Data[1] = static_cast<uint32>(Applied.Stage);
		Entry.Data[2] = static_cast<uint32>(DefIndex);
		Entry.Data[3] = static_cast<uint32>(Serial);
		Entry.Flags |= bDebug ? RoundLogFlags::Debug : 0;
	});
	PushView(Team);
	if (Deps.RivalRelease != nullptr)
	{
		Deps.RivalRelease->ScheduleRivalRelease(static_cast<uint8>(Team), Ordinal);
	}
	return EObjectivesCompletion::CountedStage;
}

void FObjectivesCore::Append(ERoundLogType Type, FQueueKey Key, FParticipantId Participant, TFunctionRef<void(FRoundLogEntry&)> Fill)
{
	if (Deps.Round == nullptr)
	{
		return;
	}
	FRoundLogEntry Entry;
	Entry.Type = Type;
	Entry.Key = Key;
	Entry.ServerTime = Deps.Round->ServerNow();
	Entry.RoundEpoch = RoundEpoch;
	Entry.Participants[0] = Participant.Value;
	Fill(Entry);
	Deps.Round->AppendEvent(Entry);
}

void FObjectivesCore::AppendTend(FParticipantId Participant, FCourtFixtureId Court, ERoundTendResult Result, bool bLatched, FQueueKey Key)
{
	Append(ERoundLogType::TendCompleted, Key, Participant, [Result, Court, bLatched](FRoundLogEntry& Entry)
	{
		Entry.Code = static_cast<uint16>(Result);
		Entry.Data[0] = static_cast<uint32>(Court.Value);
		Entry.Flags |= bLatched ? RoundLogFlags::Latched : 0;
	});
}

void FObjectivesCore::PushView(int32 Team)
{
	if (Team < 0 || Team >= TeamCount)
	{
		return;
	}
	const FObjectiveTeamView View = BuildTeamView(Team);
	if (bViewPushed[Team] && View == LastPushedView[Team])
	{
		return; // change-only: an identical push is a no-op
	}
	LastPushedView[Team] = View;
	bViewPushed[Team] = true;
	if (Deps.Views != nullptr)
	{
		Deps.Views->PushTeamView(static_cast<uint8>(Team), View);
	}
}

// --- The team view (story oc-010) ---------------------------------------------------------------------------------------

FObjectiveTeamView FObjectivesCore::BuildTeamView(int32 Team) const
{
	FObjectiveTeamView View;
	if (!bActive || !bBound || Team < 0 || Team >= TeamCount)
	{
		return View; // AC-OC-29: no site, no stage card before Active entry
	}
	View.RoundEpoch = RoundEpoch;
	View.ChainSerial = ChainSerial;
	View.StageIndex = StageIndex[Team];
	View.CompletedDefMask = CompletedDefMask[Team]; // own prelims only (ApplyCountedCompletion never sets the final's bit)
	View.RivalPrelimCount = RivalPrelimCount[Team];
	for (int32 Def = 0; Def < Bindings.Num() && View.LiveSites.Num() < FObjectiveTeamView::MaxLiveSites; ++Def)
	{
		// Live for THIS team only: a Locked row (a later stage, the final before SP-1) and every opponent row are never Live
		// for Team, so they never enter its view.
		if (SiteStates[Team][Def] == EObjectiveSiteState::Live && IsEligible(Bindings[Def].Eligibility, Team))
		{
			FObjectiveLiveSite& Site = View.LiveSites.AddDefaulted_GetRef();
			Site.Court = Bindings[Def].Court;
			Site.DefIndex = static_cast<uint8>(Def);
			// Site.Clue: story oc-013 (the role-blind clue of the court's public ClueTag record); {0, 0} until then.
		}
	}
	return View;
}

const FObjectiveTeamView& FObjectivesCore::GetLastPushedView(int32 Team) const
{
	static const FObjectiveTeamView Empty;
	return Team >= 0 && Team < TeamCount ? LastPushedView[Team] : Empty;
}
