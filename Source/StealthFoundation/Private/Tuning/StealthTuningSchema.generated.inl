// GENERATED FILE, DO NOT EDIT. Written by the console command Stealth.Tuning.SchemaGen
// (StealthGameEditor, story tdc-041) from the UPROPERTY declarations of the tuning sub-structs
// (ADR-0004 section 2). Regenerate with
//   UnrealEditor-Cmd StealthGame.uproject -ExecCmds="Stealth.Tuning.SchemaGen,QUIT_EDITOR" -unattended -nullrhi
// or, when this table no longer compiles, tools/ci/regenerate-tuning-schema.ps1 (story tdc-042).
// Commit it with every tuning field change; bump TuningSchemaVersion when the fingerprint changes.
// Rows are in byte-wise (ordinal, case-sensitive) key order: the F-2 hash order and the snapshot
// wire order. Row fields: Key, TypeTag, Get, Set, Default, HardMin, HardMax, SafeMin, SafeMax,
// bPublic, Latch, Owner, Unit, bHasSentinel, Sentinel. Included only by StealthTuningSchema.cpp.

namespace StealthTuningGenerated
{
	using StealthTuningAccess::Get;
	using StealthTuningAccess::Set;

	inline constexpr FTuningSchemaRow Rows[] = {
		{ "AssassinationRangeCm", StealthTuningTypeTag::Float, &Get<&FStealthServerTuning::Assassination_Public, &FAssassinationTuning_Public::AssassinationRangeCm>, &Set<&FStealthServerTuning::Assassination_Public, &FAssassinationTuning_Public::AssassinationRangeCm>, 200.0f, 100.0f, 250.0f, 150.0f, 250.0f, true, ETuningLatch::Use, "Assassination", "Cm", false, 0.0 },
		{ "DefenceCooldownSec", StealthTuningTypeTag::Float, &Get<&FStealthServerTuning::Assassination_Public, &FAssassinationTuning_Public::DefenceCooldownSec>, &Set<&FStealthServerTuning::Assassination_Public, &FAssassinationTuning_Public::DefenceCooldownSec>, 60.0f, 30.0f, 120.0f, 45.0f, 75.0f, true, ETuningLatch::Action, "Assassination", "Sec", false, 0.0 },
		{ "DefenceDurationSec", StealthTuningTypeTag::Float, &Get<&FStealthServerTuning::Assassination_Public, &FAssassinationTuning_Public::DefenceDurationSec>, &Set<&FStealthServerTuning::Assassination_Public, &FAssassinationTuning_Public::DefenceDurationSec>, 4.0f, 3.0f, 5.0f, 3.0f, 5.0f, true, ETuningLatch::Action, "Assassination", "Sec", false, 0.0 },
		{ "NpcCount", StealthTuningTypeTag::Int, &Get<&FStealthServerTuning::Crowd_Server, &FCrowdTuning_Server::NpcCount>, &Set<&FStealthServerTuning::Crowd_Server, &FCrowdTuning_Server::NpcCount>, 50.0, 6.0, 100.0, 45.0, 50.0, false, ETuningLatch::Round, "Crowd", "Count", false, 0.0 },
		{ "NpcKillStrikeLimit", StealthTuningTypeTag::Int, &Get<&FStealthServerTuning::Penalty_Public, &FPenaltyTuning_Public::NpcKillStrikeLimit>, &Set<&FStealthServerTuning::Penalty_Public, &FPenaltyTuning_Public::NpcKillStrikeLimit>, 3.0, 2.0, 5.0, 2.0, 4.0, true, ETuningLatch::Round, "Penalty", "Limit", false, 0.0 },
		{ "ObjectiveInteractSec", StealthTuningTypeTag::Float, &Get<&FStealthServerTuning::Objectives_Public, &FObjectivesTuning_Public::ObjectiveInteractSec>, &Set<&FStealthServerTuning::Objectives_Public, &FObjectivesTuning_Public::ObjectiveInteractSec>, 3.0f, 2.0f, 6.0f, 2.9f, 4.5f, true, ETuningLatch::Action, "Objectives", "Sec", false, 0.0 },
		{ "TargetRoundMinutes", StealthTuningTypeTag::Float, &Get<&FStealthServerTuning::Round_Server, &FRoundTuning_Server::TargetRoundMinutes>, &Set<&FStealthServerTuning::Round_Server, &FRoundTuning_Server::TargetRoundMinutes>, 12.0f, 5.0f, 30.0f, 8.0f, 15.0f, false, ETuningLatch::Round, "Round", "Minutes", false, 0.0 },
		{ "TeamSize", StealthTuningTypeTag::Int, &Get<&FStealthServerTuning::Round_Public, &FRoundTuning_Public::TeamSize>, &Set<&FStealthServerTuning::Round_Public, &FRoundTuning_Public::TeamSize>, 5.0, 1.0, 5.0, 2.0, 5.0, true, ETuningLatch::Round, "Round", "Size", false, 0.0 },
	};

	/** Schema fingerprint over names, types, audiences, latches and enum values (story tdc-043). */
	inline constexpr uint64 SchemaFingerprint = 0x4250230bdd621e89ull;
}
