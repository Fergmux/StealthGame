// Story cs-012: the eye-origin visibility mask core (GDD camera-system.md F3). See View/Core/StealthEyeMaskCore.h.

#include "View/Core/StealthEyeMaskCore.h"

#include "View/Tuning/StealthCameraParams.h"

FEyeMaskCore::FEyeMaskCore(const FStealthCameraParams& InParams)
	: NearCm(InParams.EyeMaskNearCm)
	, RangeCm(InParams.EyeMaskRangeCm)
	, NearHz(InParams.EyeMaskNearHz)
	, FarHz(InParams.EyeMaskFarHz)
	, BeyondHz(InParams.EyeMaskBeyondHz)
	, ConeHalfHDeg(InParams.EyeConeHalfHDeg)
	, ConeHalfVDeg(InParams.EyeConeHalfVDeg)
	, HideFailCount(FMath::Max(InParams.EyeMaskHideFailCount, 1))
	, FadeSec(InParams.EyeMaskFadeSec)
{
}

int32 FEyeMaskCore::Register()
{
	FTargetState& State = Targets.AddDefaulted_GetRef();
	State.bRegistered = true;
	return Targets.Num() - 1;
}

void FEyeMaskCore::Unregister(int32 Id)
{
	if (Targets.IsValidIndex(Id))
	{
		Targets[Id] = FTargetState(); // unregistered; pending handles are simply never polled again
	}
}

float FEyeMaskCore::GetAlpha(int32 Id) const
{
	return Targets.IsValidIndex(Id) ? Targets[Id].Alpha : 0.0f;
}

int32 FEyeMaskCore::GetTestsStarted(int32 Id) const
{
	return Targets.IsValidIndex(Id) ? Targets[Id].TestsStarted : 0;
}

bool FEyeMaskCore::IsInCone(const FEyeMaskView& View, const FVector& Point) const
{
	const FVector Local = View.Rotation.UnrotateVector(Point - View.Origin);
	if (Local.X <= 0.0)
	{
		return false;
	}
	const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
	const double Pitch = FMath::RadiansToDegrees(FMath::Atan2(Local.Z, Local.X));
	return FMath::Abs(Yaw) <= ConeHalfHDeg && FMath::Abs(Pitch) <= ConeHalfVDeg;
}

double FEyeMaskCore::TestPeriodSec(double DistanceCm) const
{
	const double Hz = DistanceCm <= NearCm ? NearHz : (DistanceCm <= RangeCm ? FarHz : BeyondHz);
	return Hz > 0.0 ? 1.0 / Hz : TNumericLimits<double>::Max();
}

void FEyeMaskCore::ApplyResult(FTargetState& State, bool bPass) const
{
	if (bPass)
	{
		State.bVisible = true;
		State.Fails = 0;
		return;
	}
	++State.Fails;
	State.bVisible = State.bVisible && State.Fails < HideFailCount;
}

void FEyeMaskCore::Step(double NowSec, double DtSec, const FEyeMaskView& View, FProbeProvider Probes, IEyeMaskTraceBackend& Backend)
{
	// 1. Landed results: the first clear trace passes the test; all blocked fails it.
	for (FTargetState& State : Targets)
	{
		if (!State.bRegistered || !State.bInFlight)
		{
			continue;
		}
		bool bAnyClear = false;
		bool bAllLanded = true;
		for (int32 Index = 0; Index < State.NumPending; ++Index)
		{
			if (State.bLanded[Index])
			{
				continue;
			}
			const TOptional<bool> Blocked = Backend.PollResult(State.Pending[Index]);
			if (!Blocked.IsSet())
			{
				bAllLanded = false;
				continue;
			}
			State.bLanded[Index] = true;
			bAnyClear |= !Blocked.GetValue();
		}
		if (bAnyClear || bAllLanded)
		{
			State.bInFlight = false;
			ApplyResult(State, bAnyClear);
		}
	}

	// 2. Due tests, round-robin from the cursor, within the per-frame trace cap.
	LastFrameTraces = 0;
	const int32 Count = Targets.Num();
	int32 Visited = 0;
	int32 NextCursor = Cursor;
	bool bCapHit = false;
	FEyeMaskProbes TargetProbes;
	for (; Visited < Count && !bCapHit; ++Visited)
	{
		const int32 Id = (Cursor + Visited) % FMath::Max(Count, 1);
		FTargetState& State = Targets[Id];
		if (!State.bRegistered || State.bInFlight)
		{
			continue;
		}
		TargetProbes = FEyeMaskProbes();
		if (!Probes(Id, TargetProbes) || TargetProbes.Num == 0)
		{
			continue;
		}
		const double Distance = FVector::Dist(View.Origin, TargetProbes.Points[0]);
		if (NowSec + 1e-6 < State.LastTestSec + TestPeriodSec(Distance))
		{
			continue;
		}
		int32 InCone[FEyeMaskProbes::MaxProbes];
		int32 NumInCone = 0;
		for (int32 Index = 0; Index < TargetProbes.Num; ++Index)
		{
			if (IsInCone(View, TargetProbes.Points[Index]))
			{
				InCone[NumInCone++] = Index;
			}
		}
		if (NumInCone > 0 && LastFrameTraces + NumInCone > TracesPerFrameMax)
		{
			bCapHit = true; // this target and the rest wait one frame; the next frame starts here
			NextCursor = Id;
			break;
		}
		State.LastTestSec = NowSec;
		++State.TestsStarted;
		if (NumInCone == 0)
		{
			ApplyResult(State, false); // no probe in the cone: not visible, no trace
			continue;
		}
		State.bInFlight = true;
		State.NumPending = NumInCone;
		for (int32 Index = 0; Index < NumInCone; ++Index)
		{
			State.Pending[Index] = Backend.SubmitTrace(View.Origin, TargetProbes.Points[InCone[Index]]);
			State.bLanded[Index] = false;
		}
		LastFrameTraces += NumInCone;
	}
	if (!bCapHit)
	{
		NextCursor = Count > 0 ? (Cursor + 1) % Count : 0; // rotate the start so no index is always first
	}
	Cursor = NextCursor;

	// 3. Alpha.
	const float Rate = FadeSec > 0.0 ? static_cast<float>(DtSec / FadeSec) : 1.0f;
	for (FTargetState& State : Targets)
	{
		if (State.bRegistered)
		{
			State.Alpha = FMath::Clamp(State.Alpha + (State.bVisible ? Rate : -Rate), 0.0f, 1.0f);
		}
	}
}
