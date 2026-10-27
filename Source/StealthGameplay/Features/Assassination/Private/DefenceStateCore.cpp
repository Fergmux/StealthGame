// Story asn-003 (production/epics/assassination-defence-reversal/story-003-defence-state-component-and-cooldown.md):
// F3 cooldown from activation, the protected window by key, the lockout stamp and the ASN-R18 lifecycle. See
// Server/DefenceStateCore.h. Reference logic: the prototype FStealthRules::CanStartDefence / DefenceCooldownRemaining
// (test StealthGame.Rules.Defence), re-expressed on the per-pawn state and the queue key.

#include "Server/DefenceStateCore.h"

bool FDefenceStateCore::CanPrepare(double NowServerTime, double CooldownSec) const
{
	return !LastPrepareActivation.IsSet() || NowServerTime - LastPrepareActivation.GetValue() >= CooldownSec;
}

double FDefenceStateCore::CooldownRemaining(double NowServerTime, double CooldownSec) const
{
	return LastPrepareActivation.IsSet() ? FMath::Max(LastPrepareActivation.GetValue() + CooldownSec - NowServerTime, 0.0) : 0.0;
}

TOptional<double> FDefenceStateCore::GetCooldownEndServerTime(double CooldownSec) const
{
	return LastPrepareActivation.IsSet() ? TOptional<double>(LastPrepareActivation.GetValue() + CooldownSec) : TOptional<double>();
}

EDefenceBeginResult FDefenceStateCore::BeginPrepare(double NowServerTime, double CooldownSec, FQueueKey BeginKey)
{
	if (!CanPrepare(NowServerTime, CooldownSec))
	{
		return EDefenceBeginResult::Cooldown;
	}
	LastPrepareActivation = NowServerTime;
	Window.BeginKey = BeginKey;
	Window.EndKey = FQueueKey();
	return EDefenceBeginResult::Executed;
}

TOptional<double> FDefenceStateCore::GetCapEndServerTime(double DurationSec) const
{
	return LastPrepareActivation.IsSet() ? TOptional<double>(LastPrepareActivation.GetValue() + DurationSec) : TOptional<double>();
}

void FDefenceStateCore::EndWindow(FQueueKey EndKey)
{
	// Only the first end of an open window counts (debounce expiry and CapEnd may both arrive; F2 takes the earlier key).
	if (Window.BeginKey.IsSet() && !Window.EndKey.IsSet() && Window.BeginKey < EndKey)
	{
		Window.EndKey = EndKey;
	}
}

bool FDefenceStateCore::IsWindowOpenAt(FQueueKey AttackKey) const
{
	if (!Window.BeginKey.IsSet() || !(Window.BeginKey < AttackKey))
	{
		return false;
	}
	return !(Window.EndKey.IsSet() && Window.EndKey < AttackKey);
}

void FDefenceStateCore::BeginLockout(double ExecServerTime, double LockoutSec)
{
	LockoutEnd = ExecServerTime + LockoutSec;
}

void FDefenceStateCore::AdoptForRecovery(const FDefenceStateCore* LostPawnState, double NowServerTime)
{
	if (LostPawnState != nullptr && LostPawnState->LastPrepareActivation.IsSet())
	{
		LastPrepareActivation = LostPawnState->LastPrepareActivation;
	}
	else
	{
		LastPrepareActivation = NowServerTime; // unknown stamp: stamp now, never a free brace
	}
	Window = FDefenceWindow();
}

void FDefenceStateCore::Reset()
{
	LastPrepareActivation.Reset();
	LockoutEnd = -TNumericLimits<double>::Max();
	Window = FDefenceWindow();
}
