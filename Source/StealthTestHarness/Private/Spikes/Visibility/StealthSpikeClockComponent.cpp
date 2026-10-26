// THROWAWAY spike code for the IVP S10 re-run. See StealthSpikeClockComponent.h.
#include "Spikes/Visibility/StealthSpikeClockComponent.h"

#include "Scenarios/StealthHarnessRun.h"

#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"

namespace StealthSpikeClock
{
	constexpr int32 Window = 16;
	constexpr int32 SyncSamples = 5;
	constexpr double SlewPerSecond = 0.050;
	constexpr double StepAbove = 0.250;
	constexpr double FastPeriod = 0.5;
	constexpr double FastFor = 5.0;
	constexpr double SlowPeriod = 2.0;
	constexpr int32 MaxPingsPerSecond = 4;
}

double FStealthSpikeClockEstimator::TargetOffset() const
{
	const FStealthSpikeClockSample* Best = nullptr;
	for (const FStealthSpikeClockSample& Sample : Samples)
	{
		if (!Best || Sample.Delay < Best->Delay)
		{
			Best = &Sample;
		}
	}
	return Best ? Best->Offset : 0.0;
}

void FStealthSpikeClockEstimator::AddSample(double T1, double T2, double T3, double T4)
{
	Advance(T4);
	FStealthSpikeClockSample Sample;
	Sample.Delay = (T4 - T1) - (T3 - T2);
	Sample.Offset = ((T2 - T1) + (T3 - T4)) * 0.5;
	Samples.Add(Sample);
	if (Samples.Num() > StealthSpikeClock::Window)
	{
		Samples.RemoveAt(0);
	}
	++SampleCount;
	const double Target = TargetOffset();
	if (!bSynced)
	{
		SlewedOffset = Target; // not used before sync; the first sync is the one allowed step
		if (SampleCount >= StealthSpikeClock::SyncSamples)
		{
			bSynced = true;
			++Steps;
		}
	}
	else if (FMath::Abs(Target - SlewedOffset) > StealthSpikeClock::StepAbove)
	{
		SlewedOffset = Target;
		++Steps;
	}
	if (StealthHarness::FRun* Run = StealthHarness::FRun::Get())
	{
		Run->Write(TEXT("clkPong"), FString::Printf(TEXT("\"t1\":%.6f,\"t2\":%.6f,\"t3\":%.6f,\"t4\":%.6f,\"delay\":%.6f,\"offset\":%.6f,\"target\":%.6f,\"slewed\":%.6f,\"n\":%d,\"synced\":%d"),
			T1, T2, T3, T4, Sample.Delay, Sample.Offset, Target, SlewedOffset, SampleCount, (int32)bSynced));
	}
}

void FStealthSpikeClockEstimator::Advance(double PlatformNow)
{
	if (LastSlewAt < 0.0 || !bSynced)
	{
		LastSlewAt = PlatformNow;
		return;
	}
	const double Dt = PlatformNow - LastSlewAt;
	if (Dt <= 0.0)
	{
		return;
	}
	LastSlewAt = PlatformNow;
	const double MaxStep = StealthSpikeClock::SlewPerSecond * Dt;
	SlewedOffset += FMath::Clamp(TargetOffset() - SlewedOffset, -MaxStep, MaxStep);
}

double FStealthSpikeClockEstimator::EstimatedServerNow(double PlatformNow)
{
	Advance(PlatformNow);
	return PlatformNow + SlewedOffset;
}

FStealthSpikeClockEstimator& FStealthSpikeClockEstimator::Get()
{
	static FStealthSpikeClockEstimator Estimator;
	return Estimator;
}

UStealthSpikeClockComponent::UStealthSpikeClockComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork; // T1 and T3 as close to the end-of-frame flush as a tick gets
}

namespace StealthSpikeClock
{
	/**
	 * TimeSeconds - FApp::GetCurrentTime() taken after the world advanced TimeSeconds for the frame (a server tick).
	 * RPC bodies run in TickDispatch, BEFORE UWorld::Tick adds this frame's DeltaSeconds to TimeSeconds
	 * (LevelTick.cpp: BroadcastTickDispatch precedes "TimeSeconds += DeltaSeconds"), so the frame-local formula
	 * TimeSeconds + (now - frame start) would stamp T2 one frame early there (found by the S10 re-run: -8.7 ms bias).
	 * With DeltaSeconds = the frame-start difference, the anchor is constant across frames.
	 */
	static TMap<TWeakObjectPtr<const UWorld>, double>& Anchors()
	{
		static TMap<TWeakObjectPtr<const UWorld>, double> Map;
		return Map;
	}
}

void UStealthSpikeClockComponent::NoteServerFrame(const UWorld* World)
{
	if (World)
	{
		StealthSpikeClock::Anchors().Add(World, World->GetTimeSeconds() - FApp::GetCurrentTime());
	}
}

double UStealthSpikeClockComponent::ServerNowPrecise(const UWorld* World)
{
	if (!World)
	{
		return -1.0;
	}
	if (const double* Anchor = StealthSpikeClock::Anchors().Find(World))
	{
		return FPlatformTime::Seconds() + *Anchor;
	}
	return World->GetTimeSeconds() + (FPlatformTime::Seconds() - FApp::GetCurrentTime());
}

void UStealthSpikeClockComponent::BeginPlay()
{
	Super::BeginPlay();
	ClientBeganAt = FPlatformTime::Seconds();
}

void UStealthSpikeClockComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const double Now = FPlatformTime::Seconds();
	if (GetNetMode() == NM_Client)
	{
		const double Period = Now - ClientBeganAt < StealthSpikeClock::FastFor ? StealthSpikeClock::FastPeriod : StealthSpikeClock::SlowPeriod;
		if (Now - LastPingAt >= Period)
		{
			LastPingAt = Now;
			ServerClockPing(Now);
		}
		FStealthSpikeClockEstimator& Estimator = FStealthSpikeClockEstimator::Get();
		Estimator.Advance(Now);
		if (Now - LastRecordAt >= 0.5)
		{
			LastRecordAt = Now;
			if (StealthHarness::FRun* Run = StealthHarness::FRun::Get())
			{
				Run->Write(TEXT("clk"), FString::Printf(TEXT("\"est\":%.6f,\"slewed\":%.6f,\"target\":%.6f,\"n\":%d,\"synced\":%d,\"steps\":%d"),
					Now + Estimator.SlewedOffset, Estimator.SlewedOffset, Estimator.TargetOffset(), Estimator.SampleCount, (int32)Estimator.bSynced, Estimator.Steps));
			}
		}
		return;
	}
	// Server: T3 is stamped here, in the tick after the ping arrived (the ADR's drain tick).
	NoteServerFrame(GetWorld());
	for (const TPair<double, double>& Pong : PendingPongs)
	{
		ClientClockPong(Pong.Key, Pong.Value, ServerNowPrecise(GetWorld()));
	}
	PendingPongs.Reset();
}

void UStealthSpikeClockComponent::ServerClockPing_Implementation(double T1)
{
	const double T2 = ServerNowPrecise(GetWorld());
	const double Now = FPlatformTime::Seconds();
	RecentPings.RemoveAll([Now](double At) { return Now - At > 1.0; });
	if (RecentPings.Num() >= StealthSpikeClock::MaxPingsPerSecond)
	{
		++DroppedPings;
		return;
	}
	RecentPings.Add(Now);
	PendingPongs.Emplace(T1, T2);
}

void UStealthSpikeClockComponent::ClientClockPong_Implementation(double T1, double T2, double T3)
{
	FStealthSpikeClockEstimator::Get().AddSample(T1, T2, T3, FPlatformTime::Seconds());
}
