// EXPECT-ERROR: "you cannot assign to a variable that is const"
// TARGET-MODULE: StealthFoundation
// CONTROL-MODULE: StealthFoundation
// CONTROL-FILE: controls/ReadValidatedTuningControl.cpp
//
// Compile-fail probe (ADR-0008 section 7, ADR-0004 section 5; story tdc-054, GDD AC-32). A frozen
// FValidatedTuning offers no mutator: writing a member through a const FValidatedTuning& or through the
// TSharedRef<const FValidatedTuning> it is held as must not compile. EXPECT-ERROR is the text of MSVC error C3892 ("'Tuning': you cannot assign to a variable that is const"),
// copied from the first observed run (2026-10-25; a first guess, C3490, was reported failed-other by the gate).
// Positive control: controls/ReadValidatedTuningControl.cpp reads the same member through the same holders
// and must compile, so this probe cannot fail for an unrelated reason (a missing include, a renamed member).
// Never part of a normal build: the runner copies it into a disposable worktree only.

#include "Server/StealthValidatedTuning.h"

void StealthProbeMutateValidatedTuning(const FValidatedTuning& Tuning, const TSharedRef<const FValidatedTuning>& Held)
{
	Tuning.Values().Round_Public.TeamSize = 2;
	Held->Values().Round_Public.TeamSize = 2;
}
